#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <conio.h>
#include "ape_helpers.h"

#define GAMEMOVEMENT_JUMP_HEIGHT 58.495f

HANDLE g_hProcess;
BYTE* g_pJumpPrediction;
BYTE g_patchedBuffer[6];
BYTE g_nopBuffer[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
bool g_bPatched;
int g_iOldState;

DWORD g_jumpHeightPtrPtr;
DWORD g_jumpHeightPtr;
DWORD g_valuePtr;
DWORD g_floatOffsetPtrPtr;
DWORD g_oldFloatOffsetPtr;
DWORD g_floatOffsetPtr;

double g_oldJumpHeight;
double g_newJumpHeight = 2 * 800.f * GAMEMOVEMENT_JUMP_HEIGHT;

void Error(char* text)
{
	MessageBox(0, text, "ERROR", 16);
	ExitProcess(0);
}
void UpdateConsole()
{
	system("cls");
	printf("Use SCROLL LOCK to toggle prediction.\nPrediction status: %s\n", g_bPatched ? "ON" : "OFF");
}

void EnablePrediction()
{
	ReadProcessMemory(g_hProcess, g_pJumpPrediction, g_patchedBuffer, 6, NULL);
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

	if (WriteProcessMemory(g_hProcess, g_pJumpPrediction, g_nopBuffer, 6, NULL))
		g_bPatched = true;
	else Error("Game is already patched or signatures are outdated!");

	UpdateConsole();
}

void DisablePrediction(bool notify = true)
{
	DWORD Old;
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_jumpHeightPtr) & ~0xfff), 0x4000, PAGE_EXECUTE_READWRITE, &Old);
	WriteProcessMemory(g_hProcess, LPVOID(g_jumpHeightPtr), &g_oldJumpHeight, sizeof(g_oldJumpHeight), NULL);
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_jumpHeightPtr) & ~0xfff), 0x4000, Old, &Old);

	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_floatOffsetPtrPtr) & ~0xfff), 0x4000, PAGE_EXECUTE_READWRITE, &Old);
	WriteProcessMemory(g_hProcess, LPVOID(g_floatOffsetPtrPtr), &g_oldFloatOffsetPtr, sizeof(g_oldFloatOffsetPtr), NULL);
	VirtualProtectEx(g_hProcess, LPVOID(DWORD(g_floatOffsetPtrPtr) & ~0xfff), 0x4000, Old, &Old);

	if (WriteProcessMemory(g_hProcess, g_pJumpPrediction, g_patchedBuffer, 6, NULL))
	{
		if (notify)
			g_bPatched = false;
	}
	else Error("Memory access violation!");

	UpdateConsole();
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
	SetConsoleTitle("CS:S Autobhop Prediction Enabler by alkatrazbhop");

	DWORD processID;
	printf("Waiting for CS:S to start...");
	while (1)
	{
		processID = GetPIDByName("hl2.exe");
		if (processID) break;
		Sleep(1000);
	}

	g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);

	DWORD pClient;
	while (1)
	{
		pClient = DWORD(GetModuleHandleExtern(processID, "client.dll"));
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
	g_jumpHeightPtrPtr = (FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\xDD\x05\x00\x00\x00\x00\xD9\xFA\xD8\x4D\xFC\xD9\x59\x48", "xx????xxxxxxxx")) + 2;
	g_valuePtr = (FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\xC7\x45\x00\x00\x00\x00\x00\x0F\x87\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\xC8", "xx?????xx????x????xx")) + 3;
	g_floatOffsetPtrPtr = (FindPatternEx(g_hProcess, pClient, 0x200000, (PBYTE)"\xF3\x0F\x5C\x05\x00\x00\x00\x00\xF3\x0F\x11\x45\x00\xF3\x0F\x10\x80\x00\x00\x00\x00\xF3\x0F\x11\x45\x00", "xxxx????xxxx?xxxx????xxxx?")) + 4;

	SetConsoleCtrlHandler(PHANDLER_ROUTINE(&ConsoleHandler), true);
	UpdateConsole();

	while (1)
	{
		if (GetKeyState(VK_SCROLL) & 1 && !g_bPatched)
			EnablePrediction();
		else if (!(GetKeyState(VK_SCROLL) & 1) && g_bPatched)
			DisablePrediction();
		Sleep(100);
	}

	CloseHandle(g_hProcess);
	while (_getch() != VK_RETURN) {}
	return false;
}