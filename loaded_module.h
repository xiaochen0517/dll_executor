#pragma once

#include <vector>
#include <windows.h>
#include <string>


struct ModuleInfo {
	std::wstring name;
	HMODULE baseAddress;
	std::wstring path;
};

std::vector<struct ModuleInfo> GetModuleList();

struct ExportedFunctionInfo {
	DWORD functionOrdinal;
	std::string functionName;
	FARPROC functionAddress;
	bool isForwarded;
	std::string forwardedInfo;
};

std::string CheckIsForwardedExport(char* baseAddress, DWORD functionRVA, DWORD exportDirRVA, DWORD exportDirSize);

std::vector<struct ExportedFunctionInfo> GetExportedFunctionList(char* baseAddress, IMAGE_EXPORT_DIRECTORY* pExportDirectory, DWORD exportDirRVA, DWORD exportDirSize);
