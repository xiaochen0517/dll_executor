#pragma once

char* CustomLoadLibrary(const char* dllPath);

char* GetFunctionAddrByName(char* pImageBuffer, const char* functionName);
