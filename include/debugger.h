#pragma once
#include <windows.h>
#include <pipe_server.h>
#include <pipe_constants.h>
#include <string>
#include <map>

class Debugger {
public:
	Debugger(DWORD pid);
private:
	DWORD pid = 0;
	std::map<DWORDLONG, char> replacedCharsByAddr;
	std::map<DWORDLONG, std::string> namesByAddr;
	PipeServer* pipe = NULL;
	bool initPipeServer();
	void startDebugger();
	void pipeLoop();
	BOOL setPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
	void sendBreakpointResult(std::string name, CONTEXT context);
};