#include <Windows.h>
#include <GL/gl.h>
#include <kiero.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl2.h>
#include "imgui_hook.h"

typedef BOOL(__stdcall* wglSwapBuffers_t) (HDC hDc);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern void RenderMain();

namespace ImGuiHook 
{
	WNDPROC oWndProc;
	wglSwapBuffers_t owglSwapBuffers;

	LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
			return true;
		return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
	}

	HGLRC ImGuiWglContext;
	void InitImGuiOpenGL2(HDC hDc) 
	{
		HWND mWindow = WindowFromDC(hDc);
		oWndProc = (WNDPROC)SetWindowLongPtr(mWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);
		ImGuiWglContext = wglCreateContext(hDc);
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		// You can apply your io styles here
		ImGui_ImplWin32_Init(mWindow);
		ImGui_ImplOpenGL2_Init();
	}

	static bool initImGui = false;
	BOOL __stdcall hkwglSwapBuffers(HDC hDc)
	{
		if (!initImGui)
		{
			InitImGuiOpenGL2(hDc);
			initImGui = true;
		}

		HGLRC OldWglContext = wglGetCurrentContext();
		wglMakeCurrent(hDc, ImGuiWglContext);

		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplWin32_NewFrame();

		ImGui::NewFrame();

		RenderMain();

		ImGui::EndFrame();
		ImGui::Render();

		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		wglMakeCurrent(hDc, OldWglContext);
		return owglSwapBuffers(hDc);
	}

	wglSwapBuffers_t* get_wglSwapBuffers()
	{
		auto hMod = GetModuleHandleA("OPENGL32.dll");
		if (!hMod) return nullptr;
		return (wglSwapBuffers_t*)GetProcAddress(hMod, "wglSwapBuffers");
	}

	DWORD WINAPI Main(LPVOID lpParam)
	{
		bool initHook = false;
		do
		{
			if (kiero::init(kiero::RenderType::Auto) == kiero::Status::Success)
			{
				kiero::bind(get_wglSwapBuffers(), (void**)&owglSwapBuffers, hkwglSwapBuffers);
				initHook = true;
			}
			Sleep(250);
		} while (!initHook);
		return TRUE;
	}

	// This function may still crashes, I am working to find a fix
	void Unload()
	{
		kiero::shutdown();
	}
}


