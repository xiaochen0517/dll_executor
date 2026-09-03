#include <iostream>
#include <windows.h>

#include "dll_loader.h"

using namespace std;

int main()
{
	char* dllInfo = CustomLoadLibrary("D:\\Code\\Learn\\cpp_dll_demo\\Debug\\cpp_dll_demo.dll");
	if (dllInfo == NULL)
	{
		cout << "Failed to load DLL." << endl;
	}
	else
	{
		cout << "DLL loaded successfully." << endl;
	}
	// Add 函数 两个 int 参数，返回 int
	typedef int(*AddFunc)(int, int);
	AddFunc add = (AddFunc)GetFunctionAddrByName(dllInfo, "Add");
	int result = add(3, 5);
	printf("Add(3, 5) = %d\n", result);
	return 0;
}