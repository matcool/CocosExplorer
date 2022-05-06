#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cocos2d.h>
#include <imgui.h>
#include <MinHook.h>
#include <queue>
#include <mutex>
#include <fstream>
#include <sstream>
#include <imgui-hook.hpp>

using namespace cocos2d;

const char* getNodeName(CCNode* node) {
    return typeid(*node).name() + 6;
}

void clipboardText(const std::string& text) {
    if (!OpenClipboard(NULL)) return;
    if (!EmptyClipboard()) return;
    const auto len = text.size();
    auto mem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    memcpy(GlobalLock(mem), text.c_str(), len + 1);
    GlobalUnlock(mem);
    SetClipboardData(CF_TEXT, mem);
    CloseClipboard();
}

bool operator!=(const CCSize& a, const CCSize& b) { return a.width != b.width || a.height != b.height; }
ImVec2 operator*(const ImVec2& vec, const float m) { return {vec.x * m, vec.y * m}; }
ImVec2 operator/(const ImVec2& vec, const float m) { return {vec.x / m, vec.y / m}; }
ImVec2 operator+(const ImVec2& a, const ImVec2& b) { return {a.x + b.x, a.y + b.y}; }
ImVec2 operator-(const ImVec2& a, const ImVec2& b) { return {a.x - b.x, a.y - b.y}; }

ImVec2 toVec2(const CCPoint& a) {
    const auto size = ImGui::GetMainViewport()->Size;
    const auto winSize = CCDirector::sharedDirector()->getWinSize();
    return {
        a.x / winSize.width * size.x,
        (1.f - a.y / winSize.height) * size.y
    };
}

ImVec2 toVec2(const CCSize& a) {
    const auto size = ImGui::GetMainViewport()->Size;
    const auto winSize = CCDirector::sharedDirector()->getWinSize();
    return {
        a.width / winSize.width * size.x,
        -a.height / winSize.height * size.y
    };
}

CCNode* g_node = nullptr;

void generateTree(CCNode* node, unsigned int i = 0) {
    std::stringstream stream;
    stream << "[" << i << "] " << getNodeName(node);
    if (node->getTag() != -1)
        stream << " (" << node->getTag() << ")";
    const auto childrenCount = node->getChildrenCount();
    if (childrenCount)
        stream << " {" << childrenCount << "}";

    auto visible = node->isVisible();
    ImGui::PushStyleColor(ImGuiCol_Text, visible ? ImVec4{ 1.f, 1.f, 1.f, 1.f } : ImVec4{ 0.5f, 0.5f, 0.5f, 1.f });

    bool main = ImGui::TreeNode(node, stream.str().c_str());
    bool hovered = ImGui::IsItemHovered();
    bool attributes = main && ImGui::TreeNode(node + 1, "Attributes");
    hovered |= main && ImGui::IsItemHovered();
    if ((attributes || hovered) && ImGui::GetIO().KeyShift) {
        auto& foreground = *ImGui::GetForegroundDrawList();
        auto parent = node->getParent();
        auto anchorPoint = node->getAnchorPoint();
        auto boundingBox = node->boundingBox();
        auto bbMin = CCPoint(boundingBox.getMinX(), boundingBox.getMinY());
        auto bbMax = CCPoint(boundingBox.getMaxX(), boundingBox.getMaxY());

        auto cameraParent = node;
        while (cameraParent) {
            auto camera = cameraParent->getCamera();

            auto offsetX = 0.f;
            auto offsetY = 0.f;
            auto offsetZ = 0.f;
            camera->getEyeXYZ(&offsetX, &offsetY, &offsetZ);
            auto offset = CCPoint(offsetX, offsetY);
            // they don't have -= for some reason
            bbMin = bbMin - offset;
            bbMax = bbMax - offset;

            cameraParent = cameraParent->getParent();
        }

        auto min = toVec2(parent ? parent->convertToWorldSpace(bbMin) : bbMin);
        auto max = toVec2(parent ? parent->convertToWorldSpace(bbMax) : bbMax);
        foreground.AddRectFilled(min, max, hovered ? 0x509999FF : 0x50FFFFFF);
    }

    if (main) {
        if (attributes) {
            if (ImGui::Button("Delete")) {
                node->removeFromParentAndCleanup(true);
                ImGui::TreePop();
                ImGui::TreePop();
                return;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Child")) {
                ImGui::OpenPopup("Add Child");
            }

            if (ImGui::BeginPopupModal("Add Child")) {
                static int item = 0;
                ImGui::Combo("Node", &item, "CCNode\0CCLabelBMFont\0CCLabelTTF\0CCSprite\0CCMenuItemSpriteExtra\0");

                static int tag = -1;
                ImGui::InputInt("Tag", &tag);

                static char text[256];
                static char labelFont[256] = "bigFont.fnt";
                if (item == 1) {
                    ImGui::InputText("Text", text, 256);
                    ImGui::InputText("Font", labelFont, 256);
                }
                static int fontSize = 20;
                if (item == 2) {
                    ImGui::InputText("Text", text, 256);
                    ImGui::InputInt("Font Size", &fontSize);
                }
                static bool frame = false;
                if (item == 3 || item == 4) {
                    ImGui::InputText("Texture", text, 256);
                    ImGui::Checkbox("Frame", &frame);
                }

                ImGui::Separator();

                if (ImGui::Button("Add")) {
                    CCNode* _child = nullptr;
                    switch (item) {
                    case 0:
                        _child = CCNode::create();
                        break;
                    case 1: {
                        auto child = CCLabelBMFont::create(text, labelFont);
                        _child = child;
                        break;
                    }
                    case 2: {
                        auto child = CCLabelTTF::create(text, "Arial", fontSize);
                        _child = child;
                        break;
                    }
                    case 3: {
                        CCSprite* child;
                        if (frame)
                            child = CCSprite::createWithSpriteFrameName(text);
                        else
                            child = CCSprite::create(text);
                        _child = child;
                        break;
                    }
                    case 4: {
                        CCSprite* sprite;
                        if (frame)
                            sprite = CCSprite::createWithSpriteFrameName(text);
                        else
                            sprite = CCSprite::create(text);
                        // _child = CCMenuItemSpriteExtra::create(sprite, sprite, nullptr, nullptr);
                        break;
                    }
                    }
                    if (_child != nullptr) {
                        _child->setTag(tag);
                        node->addChild(_child);
                    }

                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::Text("Addr: 0x%p", node);
            ImGui::SameLine();
            if (ImGui::Button("Copy")) {
                std::stringstream stream;
                stream << std::uppercase << std::hex << reinterpret_cast<uintptr_t>(node);
                clipboardText(stream.str());
            }
            if (node->getUserData()) {
                ImGui::Text("User data: 0x%p", node->getUserData());
            }

            auto pos = node->getPosition();
            float _pos[2] = { pos.x, pos.y };
            ImGui::DragFloat2("Position", _pos);
            node->setPosition({ _pos[0], _pos[1] });

            float _scale[3] = { node->getScale(), node->getScaleX(), node->getScaleY() };
            ImGui::DragFloat3("Scale", _scale, 0.025);
            // amazing
            if (node->getScale() != _scale[0])
                node->setScale(_scale[0]);
            else {
                node->setScaleX(_scale[1]);
                node->setScaleY(_scale[2]);
            }

            float _rot[3] = { node->getRotation(), node->getRotationX(), node->getRotationY() };
            ImGui::DragFloat3("Rotation", _rot);
            if (node->getRotation() != _rot[0])
                node->setRotation(_rot[0]);
            else {
                node->setRotationX(_rot[1]);
                node->setRotationY(_rot[2]);
            }

            float _skew[2] = { node->getSkewX(), node->getSkewY() };
            ImGui::DragFloat2("Skew", _skew);
            node->setSkewX(_skew[0]);
            node->setSkewY(_skew[1]);

            auto anchor = node->getAnchorPoint();
            ImGui::DragFloat2("Anchor Point", &anchor.x, 0.05f, 0.f, 1.f);
            node->setAnchorPoint(anchor);

            auto contentSize = node->getContentSize();
            ImGui::DragFloat2("Content Size", &contentSize.width);
            if (contentSize != node->getContentSize())
                node->setContentSize(contentSize);

            int zOrder = node->getZOrder();
            ImGui::InputInt("Z", &zOrder);
            if (node->getZOrder() != zOrder)
                node->setZOrder(zOrder);
            
            ImGui::Checkbox("Visible", &visible);
            if (visible != node->isVisible())
                node->setVisible(visible);

            if (dynamic_cast<CCRGBAProtocol*>(node) != nullptr) {
                auto rgbaNode = dynamic_cast<CCRGBAProtocol*>(node);
                auto color = rgbaNode->getColor();
                float _color[4] = { color.r / 255.f, color.g / 255.f, color.b / 255.f, rgbaNode->getOpacity() / 255.f };
                ImGui::ColorEdit4("Color", _color);
                rgbaNode->setColor({
                    static_cast<GLubyte>(_color[0] * 255),
                    static_cast<GLubyte>(_color[1] * 255),
                    static_cast<GLubyte>(_color[2] * 255)
                });
                rgbaNode->setOpacity(_color[3] * 255);
            }
            if (dynamic_cast<CCLabelProtocol*>(node) != nullptr) {
                auto labelNode = dynamic_cast<CCLabelProtocol*>(node);
                auto labelStr = labelNode->getString();
                char text[256];
                strcpy_s(text, labelStr);
                ImGui::InputText("Text", text, 256);
                if (strcmp(text, labelStr)) {
                    labelNode->setString(text);
                }
            }

            ImGui::TreePop();
        }
        
        auto children = node->getChildren();
        for (unsigned int i = 0; i < node->getChildrenCount(); ++i) {
            auto child = children->objectAtIndex(i);
            generateTree(dynamic_cast<CCNode*>(child), i);
        }
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

bool g_showWindow = false;

void draw() {
    if (g_showWindow) {
        auto& style = ImGui::GetStyle();
        style.ColorButtonPosition = ImGuiDir_Left;

        auto director = CCDirector::sharedDirector();
        if (ImGui::Begin("cocos2d explorer", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
            auto curScene = director->getRunningScene();
            generateTree(curScene);
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
        g_showWindow = !g_showWindow;
    });
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
