cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(cocos-explorer)

add_library(cocos-explorer SHARED src/main.cpp)

target_include_directories(cocos-explorer PRIVATE
  libraries/imgui-hook
  libraries/imgui-hook/imgui
  libraries/minhook/include
  libraries/cocos-headers/cocos2dx/
  libraries/cocos-headers/cocos2dx/include
  libraries/cocos-headers/cocos2dx/kazmath/include
  libraries/cocos-headers/cocos2dx/platform/win32
  libraries/cocos-headers/cocos2dx/platform/third_party/win32
  libraries/cocos-headers/cocos2dx/platform/third_party/win32/OGLES
)

add_subdirectory(libraries/minhook)
add_subdirectory(libraries/imgui-hook)
target_link_libraries(cocos-explorer 
  ${CMAKE_SOURCE_DIR}/libraries/cocos-headers/cocos2dx/libcocos2d.lib
  imgui-hook
  minhook
  opengl32
)