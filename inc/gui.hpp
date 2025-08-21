#ifndef PNGSQ_GUI_HPP
#define PNGSQ_GUI_HPP

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "glad/glad.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "GLFW/glfw3.h"

struct wndinfo {
	GLFWwindow* window;
	float scale;
	int width, height;
	bool size_changed;
	bool nfd_init, pdf_mode;
};

// Initializes VAO and shaders
bool init_shaders_prev(void);
void deinit_shaders_prev(void);

void draw(struct image& preview, struct wndinfo& wnd);
void window_preview(struct image& img, struct wndinfo& wnd, struct config& cfg);

static inline void Tooltip(char const* text, char const* hover) {
	ImGui::TextDisabled(text);
	if (ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.0f);
		ImGui::TextUnformatted(hover);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

#endif // PNGSQ_GUI_HPP
