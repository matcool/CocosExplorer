cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(cocos-explorer)

find_file(WINDOWS_HEADER windows.h)
if(NOT WINDOWS_HEADER)
  message(FATAL_ERROR "Can't find windows.h!")
endif()

file(GLOB_RECURSE IMGUI_FILES "libraries/imgui-hook/Universal OpenGL 2 Kiero Hook/**/*.cpp")
add_library(cocos-explorer SHARED src/main.cpp ${IMGUI_FILES})

target_include_directories(cocos-explorer PRIVATE
  libraries/minhook/include
  "libraries/imgui-hook/Universal OpenGL 2 Kiero Hook/include"
  "libraries/imgui-hook/Universal OpenGL 2 Kiero Hook/include/imgui"
  libraries/cocos-headers/cocos2dx/
  libraries/cocos-headers/cocos2dx/include
  libraries/cocos-headers/cocos2dx/kazmath/include
  libraries/cocos-headers/cocos2dx/platform/win32
  libraries/cocos-headers/cocos2dx/platform/third_party/win32
  libraries/cocos-headers/cocos2dx/platform/third_party/win32/OGLES
)

add_subdirectory(libraries/minhook)
target_link_libraries(cocos-explorer minhook)
target_link_libraries(cocos-explorer ${CMAKE_SOURCE_DIR}/libraries/cocos-headers/cocos2dx/libcocos2d.lib)
target_link_libraries(cocos-explorer opengl32)