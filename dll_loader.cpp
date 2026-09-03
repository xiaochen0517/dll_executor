#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "dll_loader.h"
#include "dll_imports.h"
#include "loaded_module.h"
using namespace std;

/**
 * 获取镜像缓冲区的大小
 */
DWORD GetSizeOfImage(char* pBuffer)
{
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pBuffer;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pBuffer + pDosHeader->e_lfanew);
	IMAGE_OPTIONAL_HEADER32* pOptionalHeader = &pNtHeaders->OptionalHeader;
	return pOptionalHeader->SizeOfImage;
}

/**
 * 将文件缓冲区转换为镜像缓冲区
 */
void FileBuffer2ImageBuffer(char* pFileBuffer, char* pImageBuffer)
{
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pFileBuffer;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pFileBuffer + pDosHeader->e_lfanew);
	IMAGE_FILE_HEADER* pFileHeader = &pNtHeaders->FileHeader;
	IMAGE_OPTIONAL_HEADER32* pOptionalHeader = &pNtHeaders->OptionalHeader;
	// Copy headers
	memcpy(pImageBuffer, pFileBuffer, pOptionalHeader->SizeOfHeaders);
	// Copy sections
	for (int i = 0; i < pFileHeader->NumberOfSections; i++)
	{
		IMAGE_SECTION_HEADER* pSectionHeader = (IMAGE_SECTION_HEADER*)(pFileBuffer + pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS32) + i * sizeof(IMAGE_SECTION_HEADER));
		memcpy(pImageBuffer + pSectionHeader->VirtualAddress, pFileBuffer + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData);
	}
}

/**
 * 加载DLL文件到镜像缓冲区
 */
char* LoadDll2ImageBuffer(const char* dllPath)
{
	FILE* pFile;
	fopen_s(&pFile, dllPath, "rb");
	if (pFile == NULL)
	{
		cout << "Failed to open file." << endl;
		return NULL;
	}
	fseek(pFile, 0, SEEK_END);
	int size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	cout << "File size: " << size << " bytes" << endl;
	char* pBuffer = (char*)malloc(size);
	if (pBuffer == NULL)
	{
		cout << "Failed to allocate memory." << endl;
		fclose(pFile);
		return NULL;
	}
	memset(pBuffer, 0, size);
	fread_s(pBuffer, size, 1, size, pFile);
	// 将文件缓冲区转换为镜像缓冲区
	char* pImageBuffer = (char*)malloc(GetSizeOfImage(pBuffer));
	if (pImageBuffer == NULL)
	{
		cout << "Failed to allocate memory for image buffer." << endl;
		fclose(pFile);
		free(pBuffer);
		return NULL;
	}
	memset(pImageBuffer, 0, GetSizeOfImage(pBuffer));
	printf("Size of image: %d bytes\n", GetSizeOfImage(pBuffer));
	printf("开始转换文件缓冲区为镜像缓冲区...\n");
	FileBuffer2ImageBuffer(pBuffer, pImageBuffer);

	fclose(pFile);
	free(pBuffer);
	return pImageBuffer;
}

void ChangeRelocationTable(char* pImageBuffer)
{
	// 加载DOS头和NT头
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pImageBuffer;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pImageBuffer + pDosHeader->e_lfanew);
	// 获取可选头和重定位表
	IMAGE_OPTIONAL_HEADER32* pOptionalHeader = &pNtHeaders->OptionalHeader;
	IMAGE_DATA_DIRECTORY* pRelocationDirectory = &pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	if (pRelocationDirectory->Size == 0)
	{
		cout << "没有获取到重定位表" << endl;
		return;
	}
	// 遍历重定位表
	IMAGE_BASE_RELOCATION* pBaseRelocation = (IMAGE_BASE_RELOCATION*)(pImageBuffer + pRelocationDirectory->VirtualAddress);
	while (pBaseRelocation->VirtualAddress != 0)
	{
		// 计算重定位条目数量
		DWORD numEntries = (pBaseRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		WORD* pRelocationEntries = (WORD*)((char*)pBaseRelocation + sizeof(IMAGE_BASE_RELOCATION));
		for (DWORD i = 0; i < numEntries; i++)
		{
			if ((pRelocationEntries[i] >> 12) == IMAGE_REL_BASED_HIGHLOW)
			{
				DWORD* pAddressToPatch = (DWORD*)(pImageBuffer + pBaseRelocation->VirtualAddress + (pRelocationEntries[i] & 0x0FFF));
				DWORD oldValue = *pAddressToPatch;
				DWORD newValue = oldValue - pOptionalHeader->ImageBase + (DWORD)pImageBuffer;
				*pAddressToPatch = newValue;
			}
		}
		pBaseRelocation = (IMAGE_BASE_RELOCATION*)((char*)pBaseRelocation + pBaseRelocation->SizeOfBlock);
	}
}

void MemVirtualProtect(char* pImageBuffer)
{
	// 加载DOS头和NT头
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pImageBuffer;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pImageBuffer + pDosHeader->e_lfanew);
	IMAGE_OPTIONAL_HEADER32* pOptionalHeader = &pNtHeaders->OptionalHeader;
	// 遍历节表，设置每个节的内存保护属性
	IMAGE_SECTION_HEADER* pSectionHeader = (IMAGE_SECTION_HEADER*)((char*)pNtHeaders + sizeof(IMAGE_NT_HEADERS32));
	for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++)
	{
		DWORD oldProtect = 0;
		DWORD protect = 0;
		DWORD sectionHeaderCharacteristics = pSectionHeader[i].Characteristics;
		if (sectionHeaderCharacteristics& IMAGE_SCN_MEM_EXECUTE)
		{
			if (sectionHeaderCharacteristics & IMAGE_SCN_MEM_READ)
			{
				if (sectionHeaderCharacteristics & IMAGE_SCN_MEM_WRITE)
				{
					protect = PAGE_EXECUTE_READWRITE;
				}
				else
				{
					protect = PAGE_EXECUTE_READ;
				}
			}
			else if (sectionHeaderCharacteristics & IMAGE_SCN_MEM_WRITE)
			{
				protect = PAGE_EXECUTE_WRITECOPY;
			}
			else
			{
				protect = PAGE_EXECUTE;
			}
		}
		else
		{
			if (sectionHeaderCharacteristics & IMAGE_SCN_MEM_READ)
			{
				if (sectionHeaderCharacteristics & IMAGE_SCN_MEM_WRITE)
				{
					protect = PAGE_READWRITE;
				}
				else
				{
					protect = PAGE_READONLY;
				}
			}
			else if (sectionHeaderCharacteristics & IMAGE_SCN_MEM_WRITE)
			{
				protect = PAGE_WRITECOPY;
			}
			else
			{
				protect = PAGE_NOACCESS;
			}
		}
		VirtualProtect(pImageBuffer + pSectionHeader[i].VirtualAddress, pSectionHeader[i].Misc.VirtualSize, protect, &oldProtect);
	}
}

int RunDllMain(char* pImageBuffer)
{
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pImageBuffer;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pImageBuffer + pDosHeader->e_lfanew);
	IMAGE_OPTIONAL_HEADER32* pOptionalHeader = &pNtHeaders->OptionalHeader;
	DWORD entryPointRVA = pOptionalHeader->AddressOfEntryPoint;
	if (entryPointRVA == 0)
	{
		cout << "没有获取到入口点" << endl;
		return -1;
	}
	typedef BOOL(WINAPI* DllMainFunc)(HINSTANCE, DWORD, LPVOID);
	DllMainFunc pDllMain = (DllMainFunc)(pImageBuffer + entryPointRVA);
	BOOL result = pDllMain((HINSTANCE)pImageBuffer, DLL_PROCESS_ATTACH, NULL);
	if (!result)
	{
		cout << "调用 DllMain 失败" << endl;
		return -1;
	}
	return 0;
}

/**
 * 自定义加载DLL文件
 */
char* CustomLoadLibrary(const char* dllPath)
{
	// 1. 加载 DLL 并将其转为镜像缓冲区
	char* pImageBuffer = LoadDll2ImageBuffer(dllPath);
	if (pImageBuffer == NULL)
	{
		return NULL;
	}
	// 2. 加载 DLL 的导入表并修改 IAT
	LoadImportsAndIAT(pImageBuffer);
	// 3. 计算并修改重定位表
	ChangeRelocationTable(pImageBuffer);
	// 4. 设置地址保护
	MemVirtualProtect(pImageBuffer);
	// 5. 调用 DLL 的入口点函数 DllMain
	int result = RunDllMain(pImageBuffer);
	if (result != 0)
	{
		cout << "调用 DllMain 失败" << endl;
		free(pImageBuffer);
		return NULL;
	}
	return pImageBuffer;
}


char* GetFunctionAddrByName(char* pImageBuffer, const char* functionName)
{
	// 加载 DOS 头和 NT 头
	IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pImageBuffer;
	IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pImageBuffer + pDosHeader->e_lfanew);
	IMAGE_OPTIONAL_HEADER32* pOptionalHeader = &pNtHeaders->OptionalHeader;
	DWORD exportDirRVA = pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	DWORD exportDirSize = pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	if (exportDirRVA == 0 || exportDirSize == 0)
	{
		return NULL;
	}
	IMAGE_EXPORT_DIRECTORY* pExportDirectory = (IMAGE_EXPORT_DIRECTORY*)(pImageBuffer + exportDirRVA);
	std::vector<struct ExportedFunctionInfo> functionList = GetExportedFunctionList(pImageBuffer, pExportDirectory, exportDirRVA, exportDirSize);
	for (const auto& func : functionList)
	{
		if (func.functionName == functionName)
		{
			return (char*)func.functionAddress;
		}
	}
	return NULL;
}
