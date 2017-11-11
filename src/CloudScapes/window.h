#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:console")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#elif defined(__linux__)
#include <xcb/xcb.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct GLFWwindow;
struct GLFWwindow* GetGLFWWindow();

void InitializeWindow(int width, int height, const char* name);
bool ShouldQuit();
void DestroyWindow();