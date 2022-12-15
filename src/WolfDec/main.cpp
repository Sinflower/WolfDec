#include <DXArchive.h>
#include <DXArchiveVer5.h>
#include <DXArchiveVer6.h>
#include <FileLib.h>
#include <windows.h>
#include <iostream>
#include <string>

////////////////////////
// WolfDec Version 0.3.1
////////////////////////

std::basic_ostream <TCHAR>& tcout =
#ifdef _UNICODE
std::wcout;
#else
std::cout;
#endif // _UNICODE

static const unsigned char KEY_2_01[] = { 0x0f, 0x53, 0xe1, 0x3e, 0x04, 0x37, 0x12, 0x17, 0x60, 0x0f, 0x53, 0xe1 };
static const unsigned char KEY_2_10[] = { 0x4c, 0xd9, 0x2a, 0xb7, 0x28, 0x9b, 0xac, 0x07, 0x3e, 0x77, 0xec, 0x4c };
static const unsigned char KEY_2_20[] = { 0x38, 0x50, 0x40, 0x28, 0x72, 0x4f, 0x21, 0x70, 0x3b, 0x73, 0x35, 0x38 };

static const char KEY_2_281[] = "WLFRPrO!p(;s5((8P@((UFWlu$#5(=";
static const unsigned char KEY_3_10[] = { 0x0F, 0x53, 0xE1, 0x3E, 0x8E, 0xB5, 0x41, 0x91, 0x52, 0x16, 0x55, 0xAE, 0x34, 0xC9, 0x8F, 0x79, 0x59, 0x2F, 0x59, 0x6B, 0x95, 0x19, 0x9B, 0x1B, 0x35, 0x9A, 0x2F, 0xDE, 0xC9, 0x7C, 0x12, 0x96, 0xC3, 0x14, 0xB5, 0x0F, 0x53, 0xE1, 0x3E, 0x8E, 0x00 };

static const char KEY_OWH_NORMAL[] = "nGui9('&1=@3#a";
static const char KEY_OWH_PLUS[] = "Ph=X3^]o2A(,1=@3#a";

static constexpr uint32_t MODES = 7;

uint32_t g_mode = -1;

bool unpackArchive(const uint32_t mode, const TCHAR* pFilePath)
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

	switch (mode)
	{
		case 0:
			failed = DXArchive_VER5::DecodeArchive(fullPath, TEXT(""), (const char*)KEY_2_01) < 0;
			break;
		case 1:
			failed = DXArchive_VER5::DecodeArchive(fullPath, TEXT(""), (const char*)KEY_2_10) < 0;
			break;
		case 2:
			failed = DXArchive_VER6::DecodeArchive(fullPath, TEXT(""), (const char*)KEY_2_20) < 0;
			break;
		case 3:
			failed = DXArchive::DecodeArchive(fullPath, TEXT(""), KEY_2_281) < 0;
			break;
		case 4:
			failed = DXArchive::DecodeArchive(fullPath, TEXT(""), reinterpret_cast<const char*>(KEY_3_10)) < 0;
			break;
		case 5:
			failed = DXArchive::DecodeArchive(fullPath, TEXT(""), KEY_OWH_NORMAL) < 0; // One Way Heroics - Normal
			break;
		case 6:
			failed = DXArchive::DecodeArchive(fullPath, TEXT(""), KEY_OWH_PLUS) < 0; // One Way Heroics - Plus
			break;
		default:
			break;
	}

	if (failed)
	{
		SetCurrentDirectory(directoryPath);
		RemoveDirectory(fileName);
	}

	return failed;
}

bool runProcess(const TCHAR* pProgName, const TCHAR* pFilePath, const uint32_t mode)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	std::wstring wstr = std::wstring(pProgName) + L" -m " + std::to_wstring(mode) + L" \"" + std::wstring(pFilePath) + L"\"";

	// Start the child process. 
	if (!CreateProcess(NULL,   // No module name (use command line)
					   const_cast<LPWSTR>(wstr.c_str()),   // Command line
					   NULL,           // Process handle not inheritable
					   NULL,           // Thread handle not inheritable
					   FALSE,          // Set handle inheritance to FALSE
					   0,              // No creation flags
					   NULL,           // Use parent's environment block
					   NULL,           // Use parent's starting directory 
					   &si,            // Pointer to STARTUPINFO structure
					   &pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		std::cerr << "CreateProcess failed (" << GetLastError() << ")." << std::endl;
		return false;
	}

	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	bool success = false;

	DWORD exit_code;
	if (FALSE == GetExitCodeProcess(pi.hProcess, &exit_code))
		std::cerr << "GetExitCodeProcess() failure: " << GetLastError() << std::endl;
	else
		success = (exit_code == 0);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return success;
}

void detectMode(const TCHAR* pProgName, const TCHAR* pFilePath)
{
	tcout << TEXT("Unpacking: ") << pFilePath << TEXT("... ");

	bool success = false;

	if (g_mode == -1)
	{
		for (uint32_t i = 0; i < MODES; i++)
		{
			success = runProcess(pProgName, pFilePath, i);
			if (success)
			{
				g_mode = i;
				break;
			}
		}
	}
	else
		success = runProcess(pProgName, pFilePath, g_mode);

	tcout << (success ? TEXT("Successful") : TEXT("Failed")) << std::endl;
}

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		std::cout << "Usage: " << argv[0] << " <.wolf-files>" << std::endl;
		return 0;
	}

	LPWSTR* szArglist;
	int nArgs;
	int ret = 0;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);


	if (argc >= 3 && std::string(argv[1]) == "-m")
	{
		uint32_t mode = std::stoi(argv[2]);
		ret = unpackArchive(mode, szArglist[3]);
	}
	else
	{
		for (int i = 1; i < nArgs; i++)
			detectMode(szArglist[0], szArglist[i]);
	}

	LocalFree(szArglist);
	return ret;
}
