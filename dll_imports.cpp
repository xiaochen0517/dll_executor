#include <windows.h>
#include <iostream>
#include <vector>
#include <map>

#include "dll_imports.h"
#include "dll_loader.h"
#include "loaded_module.h"
using namespace std;

// 检查DLL是否已加载，并加载DLL
char* CheckAndLoadDll(char* dllName)
{
	printf("检查并加载 DLL : %s\n", dllName);
	// 获取到当前进程已经加载的模块列表
	vector<ModuleInfo> moduleList = GetModuleList();
	if (moduleList.empty())
	{
		cout << "获取当前进程已加载的模块列表失败" << endl;
		return NULL;
	}
	// 遍历模块列表，检查是否已加载指定的DLL
	for (const auto& module : moduleList)
	{
		// 将宽字符串转换为多字节字符串
		size_t size_needed = 0;
		wcstombs_s(&size_needed, NULL, 0, module.name.c_str(), 0);
		char* moduleName = new char[size_needed];
		wcstombs_s(&size_needed, moduleName, size_needed, module.name.c_str(), _TRUNCATE);
		if (_stricmp(moduleName, dllName) == 0)
		{
			cout << "DLL 已加载: " << moduleName << ", 基址: " << module.baseAddress << endl;
			delete[] moduleName;
			return (char*)module.baseAddress;
		}
		delete[] moduleName;
	}
	return NULL;
}

// 依次加载导入表中的DLL
vector<ExportedFunctionInfo> LoadImportDll(char* pImageBuffer, IMAGE_IMPORT_DESCRIPTOR* importDescriptor)
{
	// 获取导入DLL的名称
	char* szDllName = pImageBuffer + importDescriptor->Name;
	char* pDllBaseAddress = CheckAndLoadDll(szDllName);
	if (pDllBaseAddress == NULL)
	{
		cout << "获取 DLL 的基址失败: " << szDllName << endl;
		return vector<ExportedFunctionInfo>();
	}
	// 获取已加载 DLL 的导出表
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pDllBaseAddress;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pDllBaseAddress + pDosHeader->e_lfanew);
	DWORD exportDirectoryRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	DWORD exportDirectorySize = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	IMAGE_EXPORT_DIRECTORY* pExportDirectory = (IMAGE_EXPORT_DIRECTORY*)(pDllBaseAddress + exportDirectoryRVA);
	vector<ExportedFunctionInfo> functionList = GetExportedFunctionList(pDllBaseAddress, pExportDirectory, exportDirectoryRVA, exportDirectorySize);
	return functionList;
}

// 解析转发导出字符串，返回 DLL 名称
std::string ParseForwardedExportDll(const std::string& forwardedInfo)
{
	// 转发导出格式: "DLL名.函数名" 或 "DLL名.#序号"
	size_t pos = forwardedInfo.find('.');
	if (pos != std::string::npos) {
		return forwardedInfo.substr(0, pos);
	}
	return "";
}

// 解析转发导出字符串，返回函数名或序号
std::string ParseForwardedExportFunction(const std::string& forwardedInfo)
{
	// 转发导出格式: "DLL名.函数名" 或 "DLL名.#序号"
	size_t pos = forwardedInfo.find('.');
	if (pos != std::string::npos) {
		return forwardedInfo.substr(pos + 1);
	}
	return "";
}

ExportedFunctionInfo* GetFunctionAddressByOrdinal(vector<ExportedFunctionInfo>& functionList, WORD ordinal)
{
	for (auto& functionInfo : functionList)
	{
		if (functionInfo.functionOrdinal == ordinal)
		{
			return &functionInfo;
		}
	}
	return NULL;
}

ExportedFunctionInfo* GetFunctionAddressByName(vector<ExportedFunctionInfo>& functionList, const char* functionName)
{
	for (auto& functionInfo : functionList)
	{
		if (functionInfo.functionName == functionName)
		{
			return &functionInfo;
		}
	}
	return NULL;
}

// 递归解析转发导出，获取真实的函数地址
FARPROC ResolveForwardedExport(const std::string& forwardedInfo)
{
	// 解析转发导出字符串
	std::string dllName = ParseForwardedExportDll(forwardedInfo);
	std::string funcInfo = ParseForwardedExportFunction(forwardedInfo);

	if (dllName.empty() || funcInfo.empty()) {
		return NULL;
	}

	// 添加 .dll 扩展名（如果没有的话）
	std::string fullDllName = dllName;
	if (fullDllName.find(".dll") == std::string::npos && 
		fullDllName.find(".DLL") == std::string::npos) {
		fullDllName += ".dll";
	}

	// 检查 DLL 是否已加载
	char* pDllBaseAddress = CheckAndLoadDll((char*)fullDllName.c_str());
	if (pDllBaseAddress == NULL) {
		printf("  [转发导出] 无法加载 DLL: %s\n", fullDllName.c_str());
		return NULL;
	}

	// 获取已加载 DLL 的导出表
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pDllBaseAddress;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pDllBaseAddress + pDosHeader->e_lfanew);
	DWORD exportDirectoryRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	DWORD exportDirectorySize = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	IMAGE_EXPORT_DIRECTORY* pExportDirectory = (IMAGE_EXPORT_DIRECTORY*)(pDllBaseAddress + exportDirectoryRVA);

	vector<ExportedFunctionInfo> functionList = GetExportedFunctionList(pDllBaseAddress, pExportDirectory, exportDirectoryRVA, exportDirectorySize);

	ExportedFunctionInfo* pFunctionInfo = NULL;

	// 判断是序号还是函数名
	if (funcInfo[0] == '#') {
		// 这是一个序号
		WORD ordinal = (WORD)atoi(funcInfo.c_str() + 1);
		pFunctionInfo = GetFunctionAddressByOrdinal(functionList, ordinal);
	} else {
		// 这是一个函数名
		pFunctionInfo = GetFunctionAddressByName(functionList, funcInfo.c_str());
	}

	if (pFunctionInfo == NULL) {
		printf("  [转发导出] 在 %s 中未找到函数: %s\n", fullDllName.c_str(), funcInfo.c_str());
		return NULL;
	}

	// 如果转发到的函数本身也是转发导出，则继续递归解析
	if (pFunctionInfo->isForwarded) {
		return ResolveForwardedExport(pFunctionInfo->forwardedInfo);
	}

	return pFunctionInfo->functionAddress;
}

void LoadImportsAndIAT(char* pImageBuffer)
{
    printf("开始进行导入表解析...\n");

    IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pImageBuffer;
    IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pImageBuffer + pDosHeader->e_lfanew);
    IMAGE_DATA_DIRECTORY* pImportDirectory = &pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR* pImportDescriptor = (IMAGE_IMPORT_DESCRIPTOR*)(pImageBuffer + pImportDirectory->VirtualAddress);

    while (pImportDescriptor->Name != 0)
    {
        char* dllName = (char*)(pImageBuffer + pImportDescriptor->Name);
        printf("\n处理导入 DLL: %s\n", dllName);

        vector<ExportedFunctionInfo> functionList = LoadImportDll(pImageBuffer, pImportDescriptor);
        if (functionList.empty())
        {
            cout << "获取导入 DLL 的函数列表失败" << endl;
            exit(1);
        }

        DWORD changeCount = 0;
        IMAGE_THUNK_DATA32* pThunkData = (IMAGE_THUNK_DATA32*)(pImageBuffer + pImportDescriptor->FirstThunk);
        IMAGE_THUNK_DATA32* pINT = (IMAGE_THUNK_DATA32*)(pImageBuffer + pImportDescriptor->OriginalFirstThunk);

        for (; pINT->u1.AddressOfData != 0; pINT++, pThunkData++)
        {
            ExportedFunctionInfo* pFunctionInfo = NULL;
            FARPROC realFunctionAddress = NULL;

            if (pINT->u1.Ordinal & IMAGE_ORDINAL_FLAG32)
            {
                WORD ordinal = IMAGE_ORDINAL32(pINT->u1.Ordinal);
                pFunctionInfo = GetFunctionAddressByOrdinal(functionList, ordinal);
                printf("  [序号 %d] -> 0x%X", ordinal, pFunctionInfo ? (DWORD)pFunctionInfo->functionAddress : 0);
            }
            else
            {
                IMAGE_IMPORT_BY_NAME* pImportByName = (IMAGE_IMPORT_BY_NAME*)(pImageBuffer + pINT->u1.AddressOfData);
                pFunctionInfo = GetFunctionAddressByName(functionList, (const char*)pImportByName->Name);
                printf("  [名称 %s] -> 0x%X", pImportByName->Name, pFunctionInfo ? (DWORD)pFunctionInfo->functionAddress : 0);
            }

            if (pFunctionInfo != NULL)
            {
                // 检查是否为转发导出
                if (pFunctionInfo->isForwarded)
                {
                    printf(" (转发导出: %s)", pFunctionInfo->forwardedInfo.c_str());
                    // 解析转发导出获取真实地址
                    realFunctionAddress = ResolveForwardedExport(pFunctionInfo->forwardedInfo);
                    if (realFunctionAddress == NULL)
                    {
                        printf("\n    ✗ 错误: 解析转发导出失败\n");
                        exit(1);
                    }
                    printf(" -> 实际地址: 0x%X", (DWORD)realFunctionAddress);
                    pThunkData->u1.Function = (DWORD)realFunctionAddress;
                }
                else
                {
                    pThunkData->u1.Function = (DWORD)(pFunctionInfo->functionAddress);
                }
                printf("\n");
                changeCount++;
            }
            else
            {
                printf("\n    ✗ 错误: 未找到函数\n");
                exit(1);
            }
        }

        printf("已解析 DLL: %s, 导入函数数量: %d\n\n", dllName, changeCount);
        pImportDescriptor++;
    }
}
