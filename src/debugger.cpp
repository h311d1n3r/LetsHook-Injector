#include <debugger.h>
#include <iostream>

using namespace std;

Debugger::Debugger(DWORD pid) {
    this->pid = pid;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 10);
    std::cout << "Starting debugger..." << std::endl;
    if (this->initPipeServer()) {
        SetConsoleTextAttribute(hConsole, 10);
        std::cout << "Pipe server synchronized !" << std::endl;
        SetConsoleTextAttribute(hConsole, 8);
        this->pipeLoop();
    }
    else {
        SetConsoleTextAttribute(hConsole, 12);
        std::cout << "Pipe server synchronization failed..." << std::endl;
        SetConsoleTextAttribute(hConsole, 8);
    }
}

bool Debugger::initPipeServer() {
    bool success;
    pipe = new PipeServer(success);
    if (!success) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, 12);
        std::cout << "An error occured while enabling the pipe server..." << std::endl;
        SetConsoleTextAttribute(hConsole, 8);
        return false;
    }
    const char sync[] = { SYNC };
    pipe->sendData((char*)sync, 1);
    char msg[BUFF_LEN];
    int timeout = 0;
    while (pipe->readData(msg) <= 0) {
        Sleep(100);
        timeout++;
        if (timeout >= 20) { //2 seconds
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 12);
            std::cout << "Pipe synchronization failed due to timeout..." << std::endl;
            SetConsoleTextAttribute(hConsole, 8);
            return false;
        }
    }
    if (msg) {
        if (msg[0] == SYNC) return true;
    }
    return false;
}

void Debugger::pipeLoop() {
    while (true) {
        char msg[BUFF_LEN];
        while (pipe->readData(msg) <= 0) Sleep(100);
        if (msg) {
            if (msg[0] == BREAKPOINT) {
                if (pipe->readData(msg) == sizeof(DWORDLONG)) {
                    DWORDLONG addr = 0;
                    memcpy(&addr, msg, sizeof(DWORDLONG));
                    int nameLen = pipe->readData(msg);
                    string name = string((const char*)msg, nameLen);
                    this->namesByAddr[addr] = name;
                    if (pipe->readData(msg) == sizeof(bool)) {
                        bool keepBp = msg[0];
                        this->keepBpByAddr[addr] = keepBp;
                    }
                    const char breakpointArr[] = { BREAKPOINT };
                    pipe->sendData((char*)breakpointArr, sizeof(breakpointArr));
                }
            }
            else if (msg[0] == START_DEBUGGER) this->startDebugger();
        }
    }
}

void Debugger::sendBreakpointResult(string name, CONTEXT context) {
    const char breakpointArr[] = { BREAKPOINT };
    pipe->sendData((char*)breakpointArr, sizeof(breakpointArr));
    pipe->sendData((char*) name.c_str(), name.length());
    for (int i = 0; i < sizeof(REGISTERS) / sizeof(DWORD64); i++) {
        DWORD64 regValue = 0;
        switch (i) {
        case 0:
            regValue = context.Rax;
            break;
        case 1:
            regValue = context.Rbx;
            break;
        case 2:
            regValue = context.Rcx;
            break;
        case 3:
            regValue = context.Rdx;
            break;
        case 4:
            regValue = context.R8;
            break;
        case 5:
            regValue = context.R9;
            break;
        case 6:
            regValue = context.R10;
            break;
        case 7:
            regValue = context.R11;
            break;
        case 8:
            regValue = context.R12;
            break;
        case 9:
            regValue = context.R13;
            break;
        case 10:
            regValue = context.R14;
            break;
        case 11:
            regValue = context.R15;
            break;
        case 12:
            regValue = context.Rdi;
            break;
        case 13:
            regValue = context.Rsi;
            break;
        case 14:
            regValue = context.Rsp;
            break;
        }
        char* regAddr = (char*)&regValue;
        pipe->sendData(regAddr, sizeof(DWORD64));
    }
}

BOOL Debugger::setPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege) {
    LUID luid;
    BOOL bRet = FALSE;
    if (LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : 0;
        if (AdjustTokenPrivileges(hToken, FALSE, &tp, NULL, (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL)) bRet = (GetLastError() == ERROR_SUCCESS);
    }
    return bRet;
}

void Debugger::startDebugger() {
    if (this->namesByAddr.size() == 0) return;
    HANDLE pToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &pToken)) {
        this->setPrivilege(pToken, SE_DEBUG_NAME, TRUE);
        CloseHandle(pToken);
    }
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
    DebugActiveProcess(pid);
    DebugSetProcessKillOnExit(FALSE);
    for (const auto& addr_pair : this->namesByAddr) {
        DWORDLONG addr = addr_pair.first;
        char replacedChar;
        ReadProcessMemory(process, (LPVOID)addr, &replacedChar, sizeof(replacedChar), NULL);
        this->replacedCharsByAddr[addr] = replacedChar;
        const char breakpoint = 0xCC; //int3
        WriteProcessMemory(process, (LPVOID)addr, &breakpoint, sizeof(breakpoint), NULL);
    }
    DEBUG_EVENT debugEvent;
    const DWORD dwContinueStatus = DBG_CONTINUE;
    bool stop(false);
    while (true) {
        if (WaitForDebugEvent(&debugEvent, INFINITE)) {
            if (debugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
                EXCEPTION_RECORD record = debugEvent.u.Exception.ExceptionRecord;
                if (record.ExceptionCode == EXCEPTION_BREAKPOINT) {
                    if (this->replacedCharsByAddr.find((DWORDLONG)record.ExceptionAddress) != this->replacedCharsByAddr.end()) {
                        HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, debugEvent.dwThreadId);
                        if (thread) {
                            SuspendThread(thread);
                            CONTEXT ctx = { 0 };
                            ctx.ContextFlags = CONTEXT_ALL;
                            GetThreadContext(thread, &ctx);
                            const char replaceCode[] = { this->replacedCharsByAddr[(DWORDLONG)record.ExceptionAddress] };
                            WriteProcessMemory(process, record.ExceptionAddress, replaceCode, sizeof(replaceCode), NULL);
                            ctx.Rip--;
                            ctx.Dr0 = ctx.Dr6 = ctx.Dr7 = 0;
                            ctx.EFlags |= (1 << 8);
                            sendBreakpointResult(this->namesByAddr[(DWORDLONG)record.ExceptionAddress], ctx);
                            SetThreadContext(thread, &ctx);
                            this->lastBpAddr = (DWORDLONG)record.ExceptionAddress;
                            ResumeThread(thread);
                            if (!this->keepBpByAddr[(DWORDLONG)record.ExceptionAddress]) {
                                this->replacedCharsByAddr.erase((DWORDLONG)record.ExceptionAddress);
                                this->namesByAddr.erase((DWORDLONG)record.ExceptionAddress);
                                this->keepBpByAddr.erase((DWORDLONG)record.ExceptionAddress);
                                if (this->replacedCharsByAddr.size() == 0) stop = true;
                            }
                        }
                    }
                }
                else if (record.ExceptionCode == EXCEPTION_SINGLE_STEP) {
                    if(stop) {
                        ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, dwContinueStatus);
                        DebugActiveProcessStop(pid);
                        break;
                    }
                    else if (this->lastBpAddr && this->keepBpByAddr.find(this->lastBpAddr) != this->keepBpByAddr.end()) {
                        if(this->keepBpByAddr[this->lastBpAddr]) {
                            const char breakpoint = 0xCC; //int3
                            WriteProcessMemory(process, (LPVOID)this->lastBpAddr, &breakpoint, sizeof(breakpoint), NULL);
                        }
                    }
                }
            }
            ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, dwContinueStatus);
        }
        else {
            this->replacedCharsByAddr.clear();
            this->namesByAddr.clear();
            this->keepBpByAddr.clear();
            DebugActiveProcessStop(pid);
            break;
        }
    }
}