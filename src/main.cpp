#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "nativefiledialog-extended/src/include/nfd.h"

#include "head.hpp"
#include "gui.hpp" // Includes Dear ImGui/glad/GLFW headers
#include "perspective.hpp"
#include "random.hpp"

static void glfw_err_cb(int error_code, const char* description) {
	std::fprintf(stderr, "GLFW error (%d): %s\n", error_code, description);
}

static void glfw_scale_cb(GLFWwindow* window, float x, float y) {
	static ImGuiIO& io = ImGui::GetIO();
	float scale = std::min(x, y);
	ImFontConfig font;
	font.SizePixels = std::floorf(13.0f * scale);
	io.Fonts->ClearFonts();
	io.Fonts->AddFontDefault(&font);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(PNGSQ_GLSL_VERSION_STRING);
	ImGui::GetStyle() = ImGuiStyle();
	ImGui::GetStyle().ScaleAllSizes(scale);
}

int main(void) {
	if (NFD_Init() != NFD_OKAY) {
		std::fprintf(stderr, "%s\n", NFD_GetError());
		return EXIT_FAILURE;
	}

	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit())
		return EXIT_FAILURE;
#ifndef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif // __APPLE__
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_TRUE);
	GLFWwindow* window = glfwCreateWindow(1024, 576, "pngsquish", nullptr, nullptr);
	if (window == nullptr)
		return EXIT_FAILURE;
	glfwMakeContextCurrent(window);
	glfwSetWindowContentScaleCallback(window, glfw_scale_cb);
	glfwSwapInterval(1);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::fprintf(stderr, "Failed to load OpenGL\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();
	float xs = 0.0f, ys = 0.0f;
	glfwGetWindowContentScale(window, &xs, &ys);
	float scale = std::min(xs, ys);
	ImFontConfig font;
	font.SizePixels = std::floorf(13.0f * scale);
	io.Fonts->ClearFonts();
	io.Fonts->AddFontDefault(&font);

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(PNGSQ_GLSL_VERSION_STRING);
	// ImGui::SetColorEditOptions(ImGuiColorEditFlags_Float);

	init_rand();
	if (!init_shaders()) {
		NFD_Quit();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	struct image img = {0};

	while (!glfwWindowShouldClose(window)) {
		glfwWaitEvents();
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
			ImGui_ImplGlfw_Sleep(50);
			continue;
		}
		int w = 0, h = 0;
		glfwGetFramebufferSize(window, &w, &h);
		draw(window, w, h, img);
	}

	free_image(img);
	deinit_shaders();
	NFD_Quit();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}
