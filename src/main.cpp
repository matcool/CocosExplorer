#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cocos2d.h>
#include <imgui.h>
#include <MinHook.h>
#include <fstream>
#include <sstream>
#include <imgui-hook.hpp>
#include <string_view>
#include <imgui/misc/cpp/imgui_stdlib.h>

using namespace cocos2d;

const char* get_node_name(CCNode* node) {
	return typeid(*node).name() + 6;
}

#define public_cast(value, member) [](auto* v) { \
	class FriendClass__; \
	using T = std::remove_pointer<decltype(v)>::type; \
	class FriendeeClass__: public T { \
	protected: \
		friend FriendClass__; \
	}; \
	class FriendClass__ { \
	public: \
		auto& get(FriendeeClass__* v) { return v->member; } \
	} c; \
	return c.get(reinterpret_cast<FriendeeClass__*>(v)); \
}(value)

template <typename T, typename U>
T union_cast(U value) {
	union {
		T a;
		U b;
	} u;
	u.b = value;
	return u.a;
}

void set_clipboard_text(std::string_view text) {
	if (!OpenClipboard(NULL)) return;
	if (!EmptyClipboard()) return;
	const auto len = text.size();
	auto mem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
	memcpy(GlobalLock(mem), text.data(), len + 1);
	GlobalUnlock(mem);
	SetClipboardData(CF_TEXT, mem);
	CloseClipboard();
}

bool operator!=(const CCSize& a, const CCSize& b) { return a.width != b.width || a.height != b.height; }
ImVec2 operator*(const ImVec2& vec, const float m) { return {vec.x * m, vec.y * m}; }
ImVec2 operator/(const ImVec2& vec, const float m) { return {vec.x / m, vec.y / m}; }
ImVec2 operator+(const ImVec2& a, const ImVec2& b) { return {a.x + b.x, a.y + b.y}; }
ImVec2 operator-(const ImVec2& a, const ImVec2& b) { return {a.x - b.x, a.y - b.y}; }

static CCNode* selected_node = nullptr;
static bool reached_selected_node = false;

void render_node_tree(CCNode* node, unsigned int index = 0) {
	std::stringstream stream;
	stream << "[" << index << "] " << get_node_name(node);
	if (node->getTag() != -1)
		stream << " (" << node->getTag() << ")";
	const auto children_count = node->getChildrenCount();
	if (children_count)
		stream << " {" << children_count << "}";

	auto flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;
	if (selected_node == node) {
		flags |= ImGuiTreeNodeFlags_Selected;
		reached_selected_node = true;
	}
	if (node->getChildrenCount() == 0)
		flags |= ImGuiTreeNodeFlags_Leaf;

	ImGui::PushStyleColor(ImGuiCol_Text, node->isVisible() ? ImVec4 { 1.f, 1.f, 1.f, 1.f } : ImVec4 { 0.8f, 0.8f, 0.8f, 1.f });
	const bool is_open = ImGui::TreeNodeEx(node, flags, stream.str().c_str());
	if (ImGui::IsItemClicked()) {
		selected_node = node;
		reached_selected_node = true;
	}
	if (is_open) {
		auto children = node->getChildren();
		for (unsigned int i = 0; i < children_count; ++i) {
			auto child = children->objectAtIndex(i);
			render_node_tree(static_cast<CCNode*>(child), i);
		}
		ImGui::TreePop();
	}
	ImGui::PopStyleColor();
}

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

void render_node_properties(CCNode* node) {
	if (ImGui::Button("Delete")) {
		node->removeFromParentAndCleanup(true);
		return;
	}
	ImGui::Text("Addr: 0x%p", node);
	ImGui::SameLine();
	if (ImGui::Button("Copy")) {
		std::stringstream stream;
		stream << std::uppercase << std::hex << reinterpret_cast<uintptr_t>(node);
		set_clipboard_text(stream.str());
	}
	if (node->getUserData())
		ImGui::Text("User data: 0x%p", node->getUserData());

	const auto menu_item_node = dynamic_cast<CCMenuItem*>(node);
	if (menu_item_node) {
		const auto selector = public_cast(menu_item_node, m_pfnSelector);
		ImGui::Text("CCMenuItem selector: %p", union_cast<void*>(selector));
	}

#define GET_SET_FLOAT2(name, label) { \
	auto point = node->get##name(); \
	if (ImGui::DragFloat2(label, reinterpret_cast<float*>(&point))) \
		node->set##name(point); \
}

#define GET_SET_INT(name, label) { \
	auto value = node->get##name(); \
	if (ImGui::InputInt(label, &value)) \
		node->set##name(value); \
}

#define GET_SET_CHECKBOX(name, label) { \
	auto value = node->is##name(); \
	if (ImGui::Checkbox(label, &value)) \
		node->set##name(value); \
}

	GET_SET_FLOAT2(Position, "Position");

#define dragXYBothIDK(name, label, speed) { \
	float values[3] = { node->get##name(), node->CONCAT(get, CONCAT(name, X))(), node->CONCAT(get, CONCAT(name, Y))() }; \
	if (ImGui::DragFloat3(label, values, speed)) { \
		if (node->get##name() != values[0]) \
			node->set##name(values[0]); \
		else { \
			node->CONCAT(set, CONCAT(name, X))(values[1]); \
			node->CONCAT(set, CONCAT(name, Y))(values[2]); \
		} \
	} \
}
	dragXYBothIDK(Scale, "Scale", 0.025f);
	dragXYBothIDK(Rotation, "Rotation", 1.0f);

#undef dragXYBothIDK

	{
		auto anchor = node->getAnchorPoint();
		if (ImGui::DragFloat2("Anchor Point", &anchor.x, 0.05f, 0.f, 1.f))
			node->setAnchorPoint(anchor);
	}

	GET_SET_FLOAT2(ContentSize, "Content Size");
	GET_SET_INT(ZOrder, "Z Order");
	GET_SET_CHECKBOX(Visible, "Visible");

	const auto rgba_node = dynamic_cast<CCNodeRGBA*>(node);
	if (rgba_node) {
		auto color = rgba_node->getColor();
		float colorValues[4] = {
			color.r / 255.f,
			color.g / 255.f,
			color.b / 255.f,
			rgba_node->getOpacity() / 255.f
		};
		if (ImGui::ColorEdit4("Color", colorValues)) {
			rgba_node->setColor({
				static_cast<GLubyte>(colorValues[0] * 255),
				static_cast<GLubyte>(colorValues[1] * 255),
				static_cast<GLubyte>(colorValues[2] * 255)
			});
			rgba_node->setOpacity(static_cast<GLubyte>(colorValues[3] * 255.f));
		}
	}
	const auto label_node = dynamic_cast<CCLabelProtocol*>(node);
	if (label_node) {
		std::string str = label_node->getString();
		if (ImGui::InputTextMultiline("Text", &str, {0, 50}))
			label_node->setString(str.c_str());
	}
}

bool show_window = false;
static ImFont* g_font = nullptr;

void draw() {
	if (g_font) ImGui::PushFont(g_font);
	if (show_window) {
		if (ImGui::Begin("cocos2d explorer", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {

			const auto avail = ImGui::GetContentRegionAvail();

			ImGui::BeginChild("explorer.tree", ImVec2(avail.x * 0.5f, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

			auto node = CCDirector::sharedDirector()->getRunningScene();
			reached_selected_node = false;
			render_node_tree(node);
			
			ImGui::EndChild();

			if (!reached_selected_node)
				selected_node = nullptr;

			ImGui::SameLine();

			ImGui::BeginChild("explorer.options");

			if (selected_node)
				render_node_properties(selected_node);
			else
				ImGui::Text("Select a node to edit its properties :-)");

			ImGui::EndChild();
		}
		ImGui::End();
	}

	if (ImGui::GetTime() < 5.0) {
		ImGui::SetNextWindowPos({10, 10});
		ImGui::Begin("cocosexplorermsg", nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("Cocos explorer loaded, press K to toggle");
		ImGui::End();
	}
	if (g_font) ImGui::PopFont();
}

void init() {
	auto& style = ImGui::GetStyle();
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.WindowBorderSize = 0;
	style.ColorButtonPosition = ImGuiDir_Left;

	g_font = ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.f);

	auto colors = style.Colors;
	colors[ImGuiCol_FrameBg] = ImVec4(0.31f, 0.31f, 0.31f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.59f, 0.59f, 0.59f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.61f, 0.61f, 0.61f, 0.67f);
	colors[ImGuiCol_TitleBg] = colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.71f, 0.71f, 0.71f, 0.35f);
}

// #define _CONSOLE

DWORD WINAPI my_thread(void* hModule) {
#ifdef _CONSOLE
	AllocConsole();
	std::ofstream conout("CONOUT$", std::ios::out);
	std::ifstream conin("CONIN$", std::ios::in);
	std::cout.rdbuf(conout.rdbuf());
	std::cin.rdbuf(conin.rdbuf());
#endif

	ImGuiHook::setRenderFunction(draw);
	ImGuiHook::setToggleCallback([]() {
		show_window = !show_window;
	});
	ImGuiHook::setInitFunction(init);
	auto cocosBase = GetModuleHandleA("libcocos2d.dll");
	MH_Initialize();
	ImGuiHook::setupHooks([](void* target, void* hook, void** trampoline) {
		MH_CreateHook(target, hook, trampoline);
	});
	MH_EnableHook(MH_ALL_HOOKS);

#ifdef _CONSOLE
	std::getline(std::cin, std::string());

	// crashes lol
	// ImGui_ImplOpenGL3_Shutdown();
	// ImGui_ImplWin32_Shutdown();
	// ImGui::DestroyContext();
	
	MH_Uninitialize();

	conout.close();
	conin.close();
	FreeConsole();
	FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(hModule), 0);
#endif

	return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(module);
		CreateThread(0, 0, my_thread, module, 0, 0);
	}
	return TRUE;
}
