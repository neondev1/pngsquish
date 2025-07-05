#ifndef PNGSQ_GUI_HPP
#define PNGSQ_GUI_HPP

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "glad/glad.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "GLFW/glfw3.h"

void draw(GLFWwindow* window, int width, int height, struct image& preview);

#endif // PNGSQ_GUI_HPP
