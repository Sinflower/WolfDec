#include <DXArchive.h>
#include <DXArchiveVer5.h>
#include <DXArchiveVer6.h>
#include <FileLib.h>

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

////////////////////////
// WolfDec Version 0.3.2
////////////////////////

std::basic_ostream <TCHAR>& tcout =
#ifdef _UNICODE
std::wcout;
#else
std::cout;
#endif // _UNICODE

using DecryptFunction = int (*)(TCHAR*, const TCHAR*, const char*);

struct DecryptMode
{
	DecryptMode(const std::string& name, const DecryptFunction& decFunc, const std::vector<char> key) :
		name(name),
		decFunc(decFunc),
		key(key)
	{
	}

	DecryptMode(const std::string& name, const DecryptFunction& decFunc, const std::string& key) :
		name(name),
		decFunc(decFunc),
		key(std::begin(key), std::end(key))
	{
		this->key.push_back(0x00); // The key needs to end with 0x00 so the parser knows when to stop
	}


	DecryptMode(const std::string& name, const DecryptFunction& decFunc, const std::vector<unsigned char> key) :
		name(name),
		decFunc(decFunc)
	{
		std::copy(std::begin(key), std::end(key), std::back_inserter(this->key));
	}

	std::string name;
	DecryptFunction decFunc;
	std::vector<char> key;
};

const std::vector<DecryptMode> DECRYPT_MODES = {
	{"Wolf RPG v2.01", &DXArchive_VER5::DecodeArchive, std::vector<unsigned char>{ 0x0f, 0x53, 0xe1, 0x3e, 0x04, 0x37, 0x12, 0x17, 0x60, 0x0f, 0x53, 0xe1 } },
	{"Wolf RPG v2.10", &DXArchive_VER5::DecodeArchive, std::vector<unsigned char>{ 0x4c, 0xd9, 0x2a, 0xb7, 0x28, 0x9b, 0xac, 0x07, 0x3e, 0x77, 0xec, 0x4c } },
	{"Wolf RPG v2.20", &DXArchive_VER6::DecodeArchive, std::vector<unsigned char>{ 0x38, 0x50, 0x40, 0x28, 0x72, 0x4f, 0x21, 0x70, 0x3b, 0x73, 0x35, 0x38 } },
	{"Wolf RPG v2.281", &DXArchive::DecodeArchive, "WLFRPrO!p(;s5((8P@((UFWlu$#5(=" },
	{"Wolf RPG v3.10", &DXArchive::DecodeArchive, std::vector<unsigned char>{ 0x0F, 0x53, 0xE1, 0x3E, 0x8E, 0xB5, 0x41, 0x91, 0x52, 0x16, 0x55, 0xAE, 0x34, 0xC9, 0x8F, 0x79, 0x59, 0x2F, 0x59, 0x6B, 0x95, 0x19, 0x9B, 0x1B, 0x35, 0x9A, 0x2F, 0xDE, 0xC9, 0x7C, 0x12, 0x96, 0xC3, 0x14, 0xB5, 0x0F, 0x53, 0xE1, 0x3E, 0x8E, 0x00 } },
	{"One Way Heroics", &DXArchive::DecodeArchive, "nGui9('&1=@3#a" },
	{"One Way Heroics Plus", &DXArchive::DecodeArchive, "Ph=X3^]o2A(,1=@3#a" }
};

uint32_t g_mode = -1;

bool unpackArchive(const uint32_t mode, const TCHAR* pFilePath)
{
	TCHAR fullPath[MAX_PATH];
	TCHAR filePath[MAX_PATH];
	TCHAR directoryPath[MAX_PATH];
	TCHAR fileName[MAX_PATH];
	bool failed = true;

	if (mode >= DECRYPT_MODES.size())
	{
		std::cerr << "Specified Mode: " << mode << " out of range" << std::endl;
		return false;
	}

	const DecryptMode curMode = DECRYPT_MODES.at(mode);

	ConvertFullPath__(pFilePath, fullPath);

	AnalysisFileNameAndDirPath(fullPath, filePath, directoryPath);

	SetCurrentDirectory(directoryPath);

	AnalysisFileNameAndExeName(filePath, fileName, NULL);

	CreateDirectory(fileName, NULL);

	SetCurrentDirectory(fileName);

	failed = curMode.decFunc(fullPath, TEXT(""), curMode.key.data()) < 0;

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

	if (!CreateProcess(NULL, const_cast<LPWSTR>(wstr.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
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
		for (uint32_t i = 0; i < DECRYPT_MODES.size(); i++)
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
