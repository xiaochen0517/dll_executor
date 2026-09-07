#pragma once

struct DllInfo
{
	void* pDll;
	int size;
	DWORD originalImageBase;
};

DllInfo* CustomLoadLibrary(const char* dllPath);

void* GetFunctionAddrByName(DllInfo* pDllInfo, const char* functionName);

int FreeLibrary(DllInfo* pDllInfo);
