cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(cocos-explorer)

add_library(cocos-explorer SHARED src/main.cpp)

target_include_directories(cocos-explorer PRIVATE
  libraries/imgui-hook
  libraries/imgui-hook/imgui
  libraries/minhook/include
)

add_subdirectory(libraries/minhook)
add_subdirectory(libraries/imgui-hook)
add_subdirectory(libraries/cocos-headers)
target_link_libraries(cocos-explorer 
  cocos2d
  imgui-hook
  minhook
  opengl32
)