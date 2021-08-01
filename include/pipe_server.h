#pragma once
#include <windows.h>

class PipeServer {
public:
	PipeServer(bool& success);
	~PipeServer();
	bool sendData(char* buf, ULONG len);
	int readData(char* buf);
	bool waitingData();
private:
	HANDLE hPipe = NULL;
	bool initPipe();
};