#include <iostream>
#include <windows.h>
#include <string>
#include <libloaderapi.h>
#include <filesystem>
#pragma warning( disable : 6031) //disable getchar() warning

using namespace std;

const string VERSION = "1.2";
const string AUTHOR = "h311d1n3r";
const string REQUIRED_LIBS[] = {"asmjit.dll", "LetsHook.dll"};
const string LIBS_PATH = "./libs/";

HANDLE openProcess(int pid) {
    return OpenProcess(PROCESS_ALL_ACCESS, false, pid);
}

LPVOID allocateMemory(HANDLE process, string dll_path) {
    return VirtualAllocEx(process, NULL, dll_path.size() + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void writeDLLPath(HANDLE process, string dll_path, LPVOID addr) {
    WriteProcessMemory(process, addr, dll_path.c_str(), dll_path.size() + 1, NULL);
}

HANDLE loadDLL(HANDLE process, LPVOID addr) {
    return CreateRemoteThread(process, nullptr, NULL, (LPTHREAD_START_ROUTINE)LoadLibraryA, addr, NULL, nullptr);
}

bool ends_with(std::string const& value, std::string const& ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool injectDLL(HANDLE process, string lib_path) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 13);
    cout << "Injecting " << lib_path.substr(2) << " :" << endl;
    LPVOID allocation_start_addr = allocateMemory(process, lib_path);
    writeDLLPath(process, filesystem::absolute(lib_path).u8string(), allocation_start_addr);
    HANDLE thread = loadDLL(process, allocation_start_addr);
    if (!filesystem::exists(lib_path) || !thread) {
        SetConsoleTextAttribute(hConsole, 12);
        cout << "Unable to inject DLL..." << endl;
        return false;
    }
    else {
        SetConsoleTextAttribute(hConsole, 10);
        cout << "DLL successfully injected !" << endl;
    }
    return true;
}

int main() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 11);
    cout << "--- LetsHook Injector ---" << endl;
    cout << " Version : " << VERSION << endl;
    cout << " Author : " << AUTHOR << endl;
    cout << "-------------------------" << endl << endl;
    SetConsoleTextAttribute(hConsole, 13);
    cout << "Input Process ID : ";
    SetConsoleTextAttribute(hConsole, 10);
    int pid;
    cin >> pid;
    cin.ignore();
    HANDLE process = openProcess(pid);
    if (!process) {
        SetConsoleTextAttribute(hConsole, 12);
        cout << "Couldn't open process with PID : " << pid << " !" << endl;
        getchar();
        return -1;
    }
    for (string lib : REQUIRED_LIBS) {
        string lib_path = LIBS_PATH + lib;
        if(filesystem::exists(lib_path)) {
            if (!injectDLL(process, lib_path)) {
                getchar();
                return -1;
            }
        } else {
            SetConsoleTextAttribute(hConsole, 12);
            cout << "Couldn't inject " << lib_path << " : File not found !" << endl;
            getchar();
            return -1;
        }
    }
    for (const auto& entry : filesystem::directory_iterator(LIBS_PATH)) {
        string lib_path = entry.path().u8string();
        string upperPath = lib_path;
        for (auto& c : upperPath) c = toupper(c);
        bool isRequiredLib = false;
        for (string requiredLib : REQUIRED_LIBS) {
            string upperLib = LIBS_PATH+requiredLib;
            for (auto& c : upperLib) c = toupper(c);
            if (upperLib.compare(upperPath) == 0) {
                isRequiredLib = true;
                break;
            }
        }
        if (ends_with(upperPath, ".DLL") && !isRequiredLib) {
            if (!injectDLL(process, lib_path)) {
                getchar();
                return -1;
            }
        }
    }
    SetConsoleTextAttribute(hConsole, 10);
    cout << "All libraries were successfully injected !" << endl;
    getchar();
    return 0;
}