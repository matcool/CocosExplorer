#include "pch.h"
#include <imgui.h>
#include "opengl-imgui-hook/imgui_hook.h"
#include <minhook/include/MinHook.h>


#define _NODE_NAME(type) if (dynamic_cast<type*>(node)) return #type"";
const char* getNodeName(CCNode* node) {
    _NODE_NAME(CCMenu);
    _NODE_NAME(CCLayer);
    _NODE_NAME(CCSprite);
    return "CCNode";
}

void generateTree(CCNode* node, int i = 0) {
    //                                                ew
    if (ImGui::TreeNode(node, node->getTag() == -1 ? "[%d] %s" : "[%d] %s (%d)", i, getNodeName(node), node->getTag())) {
        if (ImGui::TreeNode(node + 1, "Attributes")) {
            auto pos = node->getPosition();
            float _pos[2] = { pos.x, pos.y };
            ImGui::InputFloat2("Position", _pos);
            node->setPosition({ _pos[0], _pos[1] });

            // depending on the order only scale works or only x and y work
            float _scale[3] = { node->getScale(), node->getScaleX(), node->getScaleY() };
            ImGui::InputFloat3("Scale", _scale);
            node->setScale(_scale[0]);
            node->setScaleX(_scale[1]);
            node->setScaleY(_scale[2]);

            float _rot[3] = { node->getRotation(), node->getRotationX(), node->getRotationY() };
            ImGui::InputFloat3("Rotation", _rot);
            node->setRotation(_rot[0]);
            node->setRotationX(_rot[1]);
            node->setRotationY(_rot[2]);

            float _skew[2] = { node->getSkewX(), node->getSkewY() };
            ImGui::InputFloat2("Skew", _skew);
            node->setSkewX(_skew[0]);
            node->setSkewY(_skew[1]);

            ImGui::TreePop();
        }
        
        auto children = node->getChildren();
        for (int i = 0; i < node->getChildrenCount(); ++i) {
            auto child = children->objectAtIndex(i);
            generateTree(dynamic_cast<CCNode*>(child), i);
        }
        ImGui::TreePop();
    }
}

void RenderMain() {
    ImGui::Begin("cocos2d explorer");
    auto director = cocos2d::CCDirector::sharedDirector();
    // thank u andre
    const bool enableTouch = !ImGui::GetIO().WantCaptureMouse;
    director->getTouchDispatcher()->setDispatchEvents(enableTouch);
    auto curScene = director->getRunningScene();
    generateTree(curScene);
    ImGui::End();
}

inline void(__thiscall* dispatchKeyboardMSG)(void* self, int key, bool down);
void __fastcall dispatchKeyboardMSGHook(void* self, void*, int key, bool down) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    dispatchKeyboardMSG(self, key, down);
}

DWORD WINAPI my_thread(void* hModule) {
    ImGuiHook::Main(hModule);
    auto cocosBase = GetModuleHandleA("libcocos2d.dll");
    auto dispatchAddr = GetProcAddress(cocosBase, "?dispatchKeyboardMSG@CCKeyboardDispatcher@cocos2d@@QAE_NW4enumKeyCodes@2@_N@Z");
    MH_CreateHook(dispatchAddr, &dispatchKeyboardMSGHook, reinterpret_cast<void**>(&dispatchKeyboardMSG));
    MH_EnableHook(dispatchAddr);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0x1000, my_thread, hModule, 0, 0);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

