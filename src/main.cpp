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

static struct wndinfo window;

static void glfw_err_cb(int error_code, const char* description) {
	std::fprintf(stderr, PNGSQ_ERROR_STRING "GLFW error %d: %s\n", error_code, description);
}

static void glfw_scale_cb(GLFWwindow* window, float x, float y) {
	static ImGuiIO& io = ImGui::GetIO();
	static ImGuiStyle& style = ImGui::GetStyle();
	::window.scale = std::min(x, y);
	ImFontConfig font;
	font.SizePixels = std::floorf(::window.scale * 13.0f);
	io.Fonts->Clear();
	io.Fonts->AddFontDefault(&font);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(PNGSQ_GLSL_VERSION_STRING);
	style = ImGuiStyle();
	style.ScaleAllSizes(::window.scale);
	style.WindowMinSize.y = 4 * font.SizePixels + 8 * style.FramePadding.y + 2 * style.WindowPadding.y + 2 * style.ItemSpacing.y;
	ImGui::StyleColorsClassic();
}

static void glfw_fb_size_cb(GLFWwindow* window, int width, int height) {
	::window.width = width;
	::window.height = height;
	::window.size_changed = true;
}

static void cleanup(void) {
	NFD_Quit();
	deinit_shaders_persp();
	deinit_shaders_prev();
	glfwDestroyWindow(::window.window);
	glfwTerminate();
}

int main(int argc, char** argv) {
	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit())
		return EXIT_FAILURE;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	::window = {
		.window = glfwCreateWindow(1024, 576, "pngsquish", nullptr, nullptr),
		.nfd_init = NFD_Init() == NFD_OKAY
	};
	if (::window.window == nullptr)
		return EXIT_FAILURE;
	glfwMakeContextCurrent(::window.window);
	glfwGetFramebufferSize(::window.window, &::window.width, &::window.height);
	glfwSetWindowContentScaleCallback(::window.window, glfw_scale_cb);
	glfwSetFramebufferSizeCallback(::window.window, glfw_fb_size_cb);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::fprintf(stderr, PNGSQ_ERROR_STRING "Failed to load OpenGL functions\n");
		cleanup();
		return EXIT_FAILURE;
	}

	init_rand();
	if (!init_shaders_persp() || !init_shaders_prev()) {
		std::fprintf(stderr, PNGSQ_ERROR_STRING "(This error message shouldn't ever appear)\n");
		cleanup();
		return EXIT_FAILURE;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	float xs = 0.0f, ys = 0.0f;
	glfwGetWindowContentScale(::window.window, &xs, &ys);
	::window.scale = std::min(xs, ys);

	ImFontConfig font;
	font.SizePixels = std::floorf(::window.scale * 13.0f);
	io.Fonts->Clear();
	io.Fonts->AddFontDefault(&font);
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(::window.scale);
	style.WindowMinSize.y = 4 * font.SizePixels + 8 * style.FramePadding.y + 2 * style.WindowPadding.y + 2 * style.ItemSpacing.y;
	ImGui::StyleColorsClassic();

	ImGui_ImplGlfw_InitForOpenGL(::window.window, true);
	ImGui_ImplOpenGL3_Init(PNGSQ_GLSL_VERSION_STRING);

	struct image img = {0};

	while (!glfwWindowShouldClose(::window.window)) {
		glfwWaitEvents();
		if (glfwGetWindowAttrib(::window.window, GLFW_ICONIFIED) != 0) {
			ImGui_ImplGlfw_Sleep(50);
			continue;
		}
		draw(img, ::window);
		glfwSwapBuffers(::window.window);
	}

	free_image(img);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	cleanup();
	return EXIT_SUCCESS;
}
