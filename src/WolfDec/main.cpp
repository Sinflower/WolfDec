#include <DXArchive.h>
#include <DXArchiveVer5.h>
#include <DXArchiveVer6.h>
#include <FileLib.h>
#include <windows.h>

const unsigned char gKEY_2_10[] = { 0x0f, 0x53, 0xe1, 0x3e, 0x04, 0x37, 0x12, 0x17, 0x60, 0x0f, 0x53, 0xe1 };
const unsigned char gKEY_2_10_2[] = { 0x4c, 0xd9, 0x2a, 0xb7, 0x28, 0x9b, 0xac, 0x07, 0x3e, 0x77, 0xec, 0x4c };
const unsigned char gKEY_2_20[] = { 0x38, 0x50, 0x40, 0x28, 0x72, 0x4f, 0x21, 0x70, 0x3b, 0x73, 0x35, 0x38 };

bool unpackArchive(const TCHAR* pFilePath)
{
	TCHAR fullPath[MAX_PATH];
	TCHAR filePath[MAX_PATH];
	TCHAR directoryPath[MAX_PATH];
	TCHAR fileName[MAX_PATH];
	bool failed = true;

	ConvertFullPath__(pFilePath, fullPath);

	AnalysisFileNameAndDirPath(fullPath, filePath, directoryPath);

	SetCurrentDirectory(directoryPath);

	AnalysisFileNameAndExeName(filePath, fileName, NULL);

	CreateDirectory(fileName, NULL);

	SetCurrentDirectory(fileName);
	_tprintf(TEXT("Unpacking: %s... "), fileName);

	failed = DXArchive_VER5::DecodeArchive(fullPath, TEXT(""), (const char*)gKEY_2_10) < 0;
	if (failed) failed = DXArchive_VER5::DecodeArchive(fullPath, TEXT(""), (const char*)gKEY_2_10_2) < 0;
	if (failed) failed = DXArchive_VER6::DecodeArchive(fullPath, TEXT(""), (const char*)gKEY_2_20) < 0;
	if (failed) failed = DXArchive::DecodeArchive(fullPath, TEXT(""), "WLFRPrO!p(;s5((8P@((UFWlu$#5(=") < 0;

	if (failed)
	{
		SetCurrentDirectory(directoryPath);
		RemoveDirectory(fileName);
	}

	_tprintf(TEXT("%s\n"), (failed ? TEXT("Failed") : TEXT("Successful")));

	return !failed;
}

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		printf("Usage: %s <.wolf-files>\n", argv[0]);
		return 0;
	}


	LPWSTR* szArglist;
	int nArgs;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

	for (int i = 1; i < nArgs; i++)
		unpackArchive(szArglist[i]);

	LocalFree(szArglist);
	return 0;
}
