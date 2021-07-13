#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <conio.h>
#include "ape_helpers.h"

HANDLE g_hProcess;
BYTE* g_pJumpPrediction;
BYTE g_patchedBuffer[6];
BYTE g_nopBuffer[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
bool g_bPatched;
int g_iOldState;

#define HARDCODED_PATH "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Source\\"

void Error(char* text)
{
	//MessageBox(0, text, "ERROR", 16);
	ExitProcess(0);
}

void UpdateConlole()
{
	system("cls");
	printf("Use F5 to toggle prediction.\nPrediction status: %s\n", g_bPatched ? "ON" : "OFF");
}

void EnablePrediction()
{
	ReadProcessMemory(g_hProcess, g_pJumpPrediction, g_patchedBuffer, 6, NULL);

	if (WriteProcessMemory(g_hProcess, g_pJumpPrediction, g_nopBuffer, 6, NULL))
		g_bPatched = true;
	else Error("Game is already patched or signatures are outdated!");

	UpdateConlole();
}

void DisablePrediction(bool notify = true)
{
	if (WriteProcessMemory(g_hProcess, g_pJumpPrediction, g_patchedBuffer, 6, NULL))
	{
		if (notify)
			g_bPatched = false;
	}
	else Error("Memory access violation!");

	UpdateConlole();
}

bool WINAPI ConsoleHandler(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_CLOSE_EVENT && g_bPatched)
		DisablePrediction(false);
	else return 1;
	return 0;
}

int main()
{
	SetConsoleTitleA("CS:S Autobhop Prediction Enabler by alkatrazbhop");

	PROCESS_INFORMATION pi = {};
	STARTUPINFOA si = {};
	CreateProcessA(HARDCODED_PATH "hl2.exe", "-steam -game cstrike -novid -console -insecure", NULL, NULL, FALSE, 0, NULL, HARDCODED_PATH, &si, &pi);

	g_hProcess = pi.hProcess;

	DWORD processID = GetProcessId(g_hProcess);

	DWORD pClient;
	while (1)
	{
		pClient = (DWORD)GetModuleHandleExtern(processID, "client.dll");
		if (pClient) break;
		Sleep(100);
	}

	DWORD pHL = (DWORD)GetModuleHandleExtern(processID, "hl2.exe");
	DWORD* pCmdLine = (DWORD*)(FindPatternEx(g_hProcess, pHL, 0x4000, (PBYTE)"\x85\xC0\x79\x08\x6A\x08", "xxxxxx") - 0x13);
	char* cmdLine = new char[255];
	ReadProcessMemory(g_hProcess, pCmdLine, &pCmdLine, sizeof(DWORD), NULL);
	ReadProcessMemory(g_hProcess, pCmdLine, &pCmdLine, sizeof(DWORD), NULL);
	ReadProcessMemory(g_hProcess, pCmdLine, cmdLine, 255, NULL);
	if (!strstr(cmdLine, " -insecure"))
		Error("-insecure key is missing!");

	g_pJumpPrediction = (BYTE*)(FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\x85\xC0\x8B\x46\x08\x0F\x84\x00\xFF\xFF\xFF\xF6\x40\x28\x02\x0F\x85\x00\xFF\xFF\xFF", "xxxxxxx?xxxxxxxxx?xxx")) + 15;

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)&ConsoleHandler, true);
	UpdateConlole();

	while (1)
	{
		if (WaitForSingleObject(g_hProcess, 0) != WAIT_TIMEOUT)
			return 0;
		if (GetKeyState(VK_F5) & 1 && !g_bPatched)
			EnablePrediction();
		else if (!(GetKeyState(VK_F5) & 1) && g_bPatched)
			DisablePrediction();
		Sleep(100);
	}

	CloseHandle(g_hProcess);
	while (_getch() != VK_RETURN) {}
	return false;
}
