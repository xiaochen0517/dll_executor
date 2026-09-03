#include <iostream>
#include <windows.h>
#include <string>
#include <vector>
#include <psapi.h>

#include "loaded_module.h"

#pragma comment(lib, "psapi.lib")

std::vector<struct ModuleInfo> GetModuleList()
{
	std::vector<struct ModuleInfo> moduleList;

	HMODULE hMods[1024];
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded = 0;

	// 获取当前进程所有模块
	if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
		std::cerr << "Failed to enum modules." << std::endl;
		return moduleList;
	}

	int moduleCount = cbNeeded / sizeof(HMODULE);

	//std::cout << "=== 当前进程加载的 DLL 信息 ===" << std::endl;
	std::cout << "当前进程加载的 DLL 数量: " << moduleCount << std::endl;

	//printf("%-2s\t%-25s\t%-15s\t%s\n", "序号", "DLL 名称", "基址", "路径");
	for (int i = 0; i < moduleCount; i++) {
		wchar_t szModPath[MAX_PATH];
		wchar_t szModName[MAX_PATH];

		// 获取完整路径
		if (GetModuleFileNameEx(hProcess, hMods[i], szModPath, MAX_PATH)) {
			// 获取 DLL 名称
			GetModuleBaseName(hProcess, hMods[i], szModName, MAX_PATH);
			//printf("[%-2d]\t%-25ws\t0x%p\t%ws\n", i, szModName, hMods[i], szModPath);
			struct ModuleInfo info;
			info.name = std::wstring(szModName, szModName + wcslen(szModName));
			info.baseAddress = hMods[i];
			info.path = std::wstring(szModPath, szModPath + wcslen(szModPath));
			moduleList.push_back(info);
		}
	}
	return moduleList;
}

std::string CheckIsForwardedExport(char* baseAddress, DWORD functionRVA, DWORD exportDirRVA, DWORD exportDirSize)
{
	// 检查是否为转发导出
	// 转发导出的 RVA 位于导出表目录范围内
	if (functionRVA >= exportDirRVA && functionRVA < exportDirRVA + exportDirSize) {
		// 这是一个转发导出，获取转发字符串
		char* forwardedString = (char*)(baseAddress + functionRVA);
		return std::string(forwardedString);
	}
	return std::string();
}

std::vector<struct ExportedFunctionInfo> GetExportedFunctionList(char* baseAddress, IMAGE_EXPORT_DIRECTORY* pExportDirectory, DWORD exportDirRVA, DWORD exportDirSize)
{
	std::vector<struct ExportedFunctionInfo> functionList;
	// 获取导出函数地址表的指针
	DWORD* pAddressOfFunctions = (DWORD*)(baseAddress + pExportDirectory->AddressOfFunctions);
	// 遍历函数地址表
	for (DWORD i = 0; i < pExportDirectory->NumberOfFunctions; ++i) {
		DWORD functionRVA = pAddressOfFunctions[i];
		struct ExportedFunctionInfo functionInfo;
		functionInfo.functionOrdinal = -1;
		functionInfo.functionName = std::string();
		functionInfo.isForwarded = false;
		functionInfo.forwardedInfo = std::string();

		// 检查是否为转发导出
		std::string forwardedString = CheckIsForwardedExport(baseAddress, functionRVA, exportDirRVA, exportDirSize);
		if (!forwardedString.empty()) {
			functionInfo.isForwarded = true;
			functionInfo.forwardedInfo = forwardedString;
			functionInfo.functionAddress = (FARPROC)(baseAddress + functionRVA);
		} else {
			functionInfo.functionAddress = (FARPROC)(baseAddress + functionRVA);
		}

		functionList.push_back(functionInfo);
	}
	// 遍历名称表和序号表
	for (DWORD i = 0; i < pExportDirectory->NumberOfNames; ++i) {
		DWORD nameRVA = ((DWORD*)baseAddress)[pExportDirectory->AddressOfNames / sizeof(DWORD) + i];
		DWORD ordinal = ((WORD*)baseAddress)[pExportDirectory->AddressOfNameOrdinals / sizeof(WORD) + i];
		if (ordinal < functionList.size()) {
			functionList[ordinal].functionOrdinal = ordinal;
			functionList[ordinal].functionName = std::string(baseAddress + nameRVA);
		}
	}
	return functionList;
}

