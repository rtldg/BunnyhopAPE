#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <conio.h>
#include "ape_helpers.h"

#define GAMEMOVEMENT_JUMP_HEIGHT 58.495f
#define LANDFIX_PATCH 0

HANDLE g_hProcess;
BYTE* g_pFUCKD3D9;
BYTE* g_pReleaseVideo;
BYTE* g_pJumpPrediction;
BYTE g_patchedBuffer[6];
BYTE g_nopBuffer[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
bool g_bPatched;
bool fullscreenhook;
int g_iOldState;
DWORD pEngine;
DWORD pD3D9;

#define HARDCODED_PATH "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Source\\"

#if LANDFIX_PATCH
DWORD g_jumpHeightPtrPtr;
DWORD g_jumpHeightPtr;
DWORD g_valuePtr;
DWORD g_floatOffsetPtrPtr;
DWORD g_oldFloatOffsetPtr;
DWORD g_floatOffsetPtr;

double g_oldJumpHeight;
double g_newJumpHeight = 2 * 800.f * GAMEMOVEMENT_JUMP_HEIGHT;
#endif

void Error(char* text)
{
	//MessageBox(0, text, "ERROR", 16);
	ExitProcess(0);
}
void UpdateConsole()
{
	system("cls");

	printf("Use F5 to toggle prediction.\nF6 to toggle fullscreenhook.\nPrediction status: %s | fullscreenhook: %s\n", g_bPatched ? "ON" : "OFF", fullscreenhook ? "ON" : "OFF");
}

void enablefullscreenhook()
{
	char d3djmp[] = { 0x90, 0xE9 };
	WriteProcessMemory(g_hProcess, g_pFUCKD3D9, d3djmp, 2, NULL);
	fullscreenhook = true;
}

void disablefullscreenhook()
{
	char d3djz[] = { 0x0f, 0x84 };
	WriteProcessMemory(g_hProcess, g_pFUCKD3D9, d3djz, 2, NULL);
	fullscreenhook = false;
}

void EnablePrediction()
{
	ReadProcessMemory(g_hProcess, g_pJumpPrediction, g_patchedBuffer, 6, NULL);

#if LANDFIX_PATCH
	ReadProcessMemory(g_hProcess, LPVOID(g_jumpHeightPtrPtr), &g_jumpHeightPtr, sizeof(g_jumpHeightPtr), NULL);
	ReadProcessMemory(g_hProcess, LPVOID(g_jumpHeightPtr), &g_oldJumpHeight, sizeof(g_oldJumpHeight), NULL);
	ReadProcessMemory(g_hProcess, LPVOID(g_floatOffsetPtrPtr), &g_oldFloatOffsetPtr, sizeof(g_oldFloatOffsetPtr), NULL);

	DWORD Old;
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_floatOffsetPtrPtr) & ~0xfff), 0x4000, PAGE_EXECUTE_READWRITE, &Old);
	WriteProcessMemory(g_hProcess, LPVOID(g_floatOffsetPtrPtr), &g_valuePtr, sizeof(g_valuePtr), NULL);
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_floatOffsetPtrPtr) & ~0xfff), 0x4000, Old, &Old);

	ReadProcessMemory(g_hProcess, LPVOID(g_floatOffsetPtrPtr), &g_floatOffsetPtr, sizeof(g_floatOffsetPtr), NULL);

	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_jumpHeightPtr) & ~0xfff), 0x4000, PAGE_EXECUTE_READWRITE, &Old);
	WriteProcessMemory(g_hProcess, LPVOID(g_jumpHeightPtr), &g_newJumpHeight, sizeof(g_newJumpHeight), NULL);
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_jumpHeightPtr) & ~0xfff), 0x4000, Old, &Old);
#endif

	if (WriteProcessMemory(g_hProcess, g_pJumpPrediction, g_nopBuffer, 6, NULL))
		g_bPatched = true;
	else Error("Game is already patched or signatures are outdated!");

	char shortjmp[] = { 0xEB };
	WriteProcessMemory(g_hProcess, g_pReleaseVideo + 12, shortjmp, 1, NULL);
}

void DisablePrediction(bool notify = true)
{
#if LANDFIX_PATCH
	DWORD Old;
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_jumpHeightPtr) & ~0xfff), 0x4000, PAGE_EXECUTE_READWRITE, &Old);
	WriteProcessMemory(g_hProcess, LPVOID(g_jumpHeightPtr), &g_oldJumpHeight, sizeof(g_oldJumpHeight), NULL);
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_jumpHeightPtr) & ~0xfff), 0x4000, Old, &Old);

	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_floatOffsetPtrPtr) & ~0xfff), 0x4000, PAGE_EXECUTE_READWRITE, &Old);
	WriteProcessMemory(g_hProcess, LPVOID(g_floatOffsetPtrPtr), &g_oldFloatOffsetPtr, sizeof(g_oldFloatOffsetPtr), NULL);
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_floatOffsetPtrPtr) & ~0xfff), 0x4000, Old, &Old);
#endif

	if (WriteProcessMemory(g_hProcess, g_pJumpPrediction, g_patchedBuffer, 6, NULL))
	{
		if (notify)
			g_bPatched = false;
	}
	else Error("Memory access violation!");

	char shortjnz[] = { 0x75 };
	WriteProcessMemory(g_hProcess, g_pReleaseVideo + 12, shortjnz, 1, NULL);
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

	pEngine = (DWORD)GetModuleHandleExtern(processID, "engine.dll");
	pD3D9 = (DWORD)GetModuleHandleExtern(processID, "d3d9.dll");

	DWORD pHL = (DWORD)GetModuleHandleExtern(processID, "hl2.exe");
	DWORD* pCmdLine = (DWORD*)(FindPatternEx(g_hProcess, pHL, 0x4000, (PBYTE)"\x85\xC0\x79\x08\x6A\x08", "xxxxxx") - 0x13);
	char* cmdLine = new char[255];
	ReadProcessMemory(g_hProcess, pCmdLine, &pCmdLine, sizeof(DWORD), NULL);
	ReadProcessMemory(g_hProcess, pCmdLine, &pCmdLine, sizeof(DWORD), NULL);
	ReadProcessMemory(g_hProcess, pCmdLine, cmdLine, 255, NULL);
	if (!strstr(cmdLine, " -insecure"))
		Error("-insecure key is missing!");

	g_pJumpPrediction = (BYTE*)(FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\x85\xC0\x8B\x46\x08\x0F\x84\x00\xFF\xFF\xFF\xF6\x40\x28\x02\x0F\x85\x00\xFF\xFF\xFF", "xxxxxxx?xxxxxxxxx?xxx")) + 15;

#if LANDFIX_PATCH
	g_jumpHeightPtrPtr = (FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\xDD\x05\x00\x00\x00\x00\xD9\xFA\xD8\x4D\xFC\xD9\x59\x48", "xx????xxxxxxxx")) + 2;
	g_valuePtr = (FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\xC7\x45\x00\x00\x00\x00\x00\x0F\x87\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\xC8", "xx?????xx????x????xx")) + 3;
	g_floatOffsetPtrPtr = (FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\xF3\x0F\x5C\x05\x00\x00\x00\x00\xF3\x0F\x11\x45\x00\xF3\x0F\x10\x80\x00\x00\x00\x00\xF3\x0F\x11\x45\x00", "xxxx????xxxx?xxxx????xxxx?")) + 4;
#endif

	g_pReleaseVideo = (BYTE*)(FindPatternEx(g_hProcess, pEngine, 0x2000000, (PBYTE)"\x56\x8B\xF1\x8B\x06\x8B\x40\x2A\xFF\xD0\x84\xC0\x75\x2A\x8B\x06", "xxxxxxx?xxxxx?xx"));
	g_pFUCKD3D9 = (BYTE*)(FindPatternEx(g_hProcess, pD3D9, 0x2000000, (PBYTE)"\x0F\x84\x2A\x2A\x2A\x2A\x6A\x07\xFF\xB3", "xx????xxxx"));

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)&ConsoleHandler, true);
	enablefullscreenhook();
	EnablePrediction();
	UpdateConsole();
	ShowWindow(GetConsoleWindow(), SW_MINIMIZE);

	bool toggle_state_f5 = false;
	bool toggle_state_f6 = false;

	while (1)
	{
		if (WaitForSingleObject(g_hProcess, 0) != WAIT_TIMEOUT)
			return 0;

		bool current_state_f5 = !!(GetKeyState(VK_F5) & 1);
		if (toggle_state_f5 != current_state_f5)
		{
			if (g_bPatched)
				DisablePrediction();
			else
				EnablePrediction();
			UpdateConsole();
		}
		toggle_state_f5 = current_state_f5;

		bool current_state_f6 = !!(GetKeyState(VK_F6) & 1);
		if (toggle_state_f6 != current_state_f6)
		{
			if (fullscreenhook)
				disablefullscreenhook();
			else
				enablefullscreenhook();
			UpdateConsole();
		}
		toggle_state_f6 = current_state_f6;

		Sleep(100);
	}

	//CloseHandle(g_hProcess);
	//while (_getch() != VK_RETURN) {}
	return false;
}
