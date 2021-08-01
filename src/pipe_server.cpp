#include "pipe_server.h"
#include "pipe_constants.h"
#include <iostream>

PipeServer::PipeServer(bool& success) {
    success = this->initPipe();
    if (success) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, 10);
        std::cout << "Pipe server initialized !" << std::endl;
        SetConsoleTextAttribute(hConsole, 8);
    }
}

PipeServer::~PipeServer() {
    if (this->hPipe) {
        const char disconnect[] = { DISCONNECT };
        this->sendData((char*)disconnect, 1);
        CloseHandle(hPipe);
    }
}

bool PipeServer::initPipe() {
    this->hPipe = CreateNamedPipe(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE |
        PIPE_READMODE_MESSAGE |
        PIPE_WAIT,
        1,
        BUFF_LEN,
        BUFF_LEN,
        0,
        NULL);
    COMMTIMEOUTS timeouts = { 10, 0, 0, 0, 0 };
    SetCommTimeouts(hPipe, &timeouts);
    if (!(ConnectNamedPipe(this->hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED))) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, 12);
        std::cout << "Client pipe couldn't connect..." << std::endl;
        SetConsoleTextAttribute(hConsole, 8);
        CloseHandle(hPipe);
        return false;
    }
    if (this->hPipe == NULL) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, 12);
        std::cout << "An error occured while trying to enable pipe : " << GetLastError() << std::endl;
        SetConsoleTextAttribute(hConsole, 8);
        return false;
    }
    return true;
}

bool PipeServer::sendData(char* buf, ULONG len) {
    ULONG writtenLen;
    if (WriteFile(this->hPipe, buf, len, &writtenLen, NULL)) {
        if (writtenLen == len) return true;
        return false;
    }
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 12);
    std::cout << "An error occured while trying to write to pipe : " << GetLastError() << std::endl;
    SetConsoleTextAttribute(hConsole, 8);
    return false;
}

int PipeServer::readData(char* buf) {
    ULONG readLen;
    if (ReadFile(this->hPipe, buf, BUFF_LEN, &readLen, NULL)) {
        return readLen;
    }
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 12);
    std::cout << "An error occured while trying to read from pipe : " << GetLastError() << std::endl;
    SetConsoleTextAttribute(hConsole, 8);
    return -1;
}

bool PipeServer::waitingData() {
    DWORD total_available_bytes;
    if (PeekNamedPipe(hPipe, 0, 0, 0, &total_available_bytes, 0)) {
        if (total_available_bytes > 0) {
            return true;
        }
    }
    return false;
}