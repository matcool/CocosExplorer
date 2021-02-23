#pragma once
#include <Windows.h>

namespace ImGuiHook {
	DWORD WINAPI Main(LPVOID lpParam);
	void Unload();
}
