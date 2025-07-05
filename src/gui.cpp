#include "head.hpp"
#include "gui.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "nativefiledialog-extended/src/include/nfd.h"

namespace {
	int prev_original_percent;
	int prev_original_width, prev_original_height;
	int prev_dewarped_percent;
	int prev_dewarped_width, prev_dewarped_height;
}

// Checks if a path has the .png extension
static bool has_ext(char const* path) {
	static const char png[] = ".png";
	const size_t len = std::strlen(path);
	if (len < 4)
		return false;
	char const* const end = path + len;
	char const* const begin = end - 4;
	return std::equal(begin, end, png, png + sizeof(png) - 1,
		[](const char& left, const char& right) { return std::toupper(left) == std::toupper(right); });
}

static void window_background(struct image& img, std::vector<struct threshold>& thresholds, struct config& cfg);
static void window_file(struct image& img, struct config& cfg);
static void window_preview(struct image& img, struct config& cfg);
static void window_processing(struct image& img, struct config& cfg);

static void tooltip(char const* text, char const* hover) {
	ImGui::TextDisabled(text);
	if (ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.0f);
		ImGui::TextUnformatted(hover);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void draw(GLFWwindow* window, int width, int height, struct image& preview) {
	static struct config cfg = {0};
	static std::vector<struct threshold> thresholds;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	window_background(preview, thresholds, cfg);
	window_file(preview, cfg);
	window_preview(preview, cfg);
	window_processing(preview, cfg);

	ImGui::Render();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(window);
}

static void window_background(struct image& img, std::vector<struct threshold>& thresholds, struct config& cfg) {
	static ImGuiStyle& style = ImGui::GetStyle();
	static float colours[6] = { 0.0f };
	if (ImGui::Begin("Background")) {
		colours[0] = std::roundf(cfg.ovr_bg_before_col.r / 2.55f) / 100.0f;
		colours[1] = std::roundf(cfg.ovr_bg_before_col.g / 2.55f) / 100.0f;
		colours[2] = std::roundf(cfg.ovr_bg_before_col.b / 2.55f) / 100.0f;
		colours[3] = std::roundf(cfg.ovr_bg_after_col.r / 2.55f) / 100.0f;
		colours[4] = std::roundf(cfg.ovr_bg_after_col.g / 2.55f) / 100.0f;
		colours[5] = std::roundf(cfg.ovr_bg_after_col.b / 2.55f) / 100.0f;
		ImGui::Checkbox("Override background before processing", &cfg.ovr_bg_before);
		ImGui::SameLine();
		if (ImGui::ColorEdit3("##before", &colours[0], ImGuiColorEditFlags_NoInputs)) {
			cfg.ovr_bg_before_col.r = (unsigned char)std::roundf(colours[0] * 255.0f);
			cfg.ovr_bg_before_col.g = (unsigned char)std::roundf(colours[1] * 255.0f);
			cfg.ovr_bg_before_col.b = (unsigned char)std::roundf(colours[2] * 255.0f);
		}
		ImGui::Checkbox("Override background after processing", &cfg.ovr_bg_after);
		ImGui::SameLine(0.0f, style.ItemSpacing.x + ImGui::CalcTextSize("before").x - ImGui::CalcTextSize("after").x);
		if (ImGui::ColorEdit3("##after", &colours[3], ImGuiColorEditFlags_NoInputs)) {
			cfg.ovr_bg_after_col.r = (unsigned char)std::roundf(colours[3] * 255.0f);
			cfg.ovr_bg_after_col.g = (unsigned char)std::roundf(colours[4] * 255.0f);
			cfg.ovr_bg_after_col.b = (unsigned char)std::roundf(colours[5] * 255.0f);
		}
		ImGui::Checkbox("Dark mode", &cfg.dark);
		ImGui::SameLine();
		tooltip("(?)", "Use this if the background of the image is darker than the text\n(e.g. text on a blackboard)");

		if (ImGui::Button("+"))
			thresholds.push_back({ false, { 0.0f, 0.0f, 0.0f }, false, true });
		ImGui::SameLine();
		if (ImGui::Button("-")) {
			auto it = std::remove_if(thresholds.begin(), thresholds.end(), [](struct threshold& thr) { return thr.selected; });
			thresholds.erase(it, thresholds.end());
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear..."))
			thresholds.clear();
		ImGui::SameLine();
		if (ImGui::Button("Reset...")) {
			// TODO
		}
		ImGui::SameLine();
		tooltip("(?)",
			"The Mode column changes how pngsquish interprets the Value slider.\n"
			"Suppose the Value slider is set to V and the background has value B.\n"
			"- Range (default): All pixels with value within B\u00B1V (and within hue/saturation ranges) are considered part of the background.\n"
			"- Compare: More aggressive; all pixels with value greater than B-V (for dark mode: value less than B+V) are considered part of the background.\n"
			"The Hue and Saturation sliders always behave under Range mode.");

		static const ImGuiTableFlags flags =
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_ScrollY;
		if (ImGui::BeginTable("thresholds", 6, flags)) {
			const float btn_width = ImGui::CalcTextSize("Compare").x + 2 * style.FramePadding.x;
			ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Select").x);
			ImGui::TableSetupColumn("Hue", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Saturation", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, btn_width);
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Enabled").x);
			ImGui::TableHeadersRow();
			struct threshold* const thrs = thresholds.data();
			for (size_t i = 0, size = thresholds.size(); i < size; i++) {
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Checkbox("##select", &thrs[i].selected);
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::SliderFloat("##h", &thrs[i].diff.h, 0.0f, 180.0f, "%.0f", ImGuiSliderFlags_ClampOnInput);
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::SliderFloat("##s", &thrs[i].diff.s, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_ClampOnInput);
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::SliderFloat("##v", &thrs[i].diff.v, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_ClampOnInput);
				ImGui::TableNextColumn();
				if (ImGui::Button(thrs[i].mode ? "Compare###mode" : "Range###mode", ImVec2(btn_width, 0.0f)))
					thrs[i].mode = !thrs[i].mode;
				ImGui::TableNextColumn();
				ImGui::Checkbox("##enabled", &thrs[i].enabled);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

static void window_file(struct image& img, struct config& cfg) {
	static const nfdu8filteritem_t in_filters[] = {
		{ "Common image formats", "png,jpg,jpeg,bmp,gif" },
		{ "PNG", "png" },
		{ "JPEG", "jpg,jpeg,jpe,jif,jfif,jfi" },
		{ "BMP", "bmp,dib" },
		{ "GIF", "gif" }
	};
	static const nfdu8filteritem_t png_filter[] = {{ "PNG", "png" }};
	static const nfdu8filteritem_t pdf_filter[] = {{ "PDF", "pdf" }};
	static ImGuiStyle& style = ImGui::GetStyle();
	static std::string in_path;
	static std::string out_path;
	static std::vector<std::string> pdf_paths;
	static bool pdf = false;
	static bool pdf_check = false;

	if (ImGui::Begin("File")) {
		float offset = ImGui::CalcTextSize("Output path").x;
		float padding = style.WindowPadding.x + style.ItemSpacing.x;
		float btn_pad = style.WindowPadding.x + 2 * style.FramePadding.x + ImGui::CalcTextSize("Browse...").x;

		if (pdf) ImGui::BeginDisabled();
		ImGui::TextUnformatted("Input path");
		ImGui::SameLine(offset, padding);
		ImGui::SetNextItemWidth(-btn_pad);
		ImGui::InputText("##in_path", &in_path);
		ImGui::SameLine();
		if (ImGui::Button("Browse...##in_browse")) {
			nfdu8char_t* path = nullptr;
			nfdopendialogu8args_t args = {0};
			args.filterList = in_filters;
			args.filterCount = 5;
			nfdresult_t res = NFD_OpenDialogU8_With(&path, &args);
			if (res == NFD_OKAY) {
				in_path = path;
				NFD_FreePathU8(path);
			}
		}
		if (pdf) ImGui::EndDisabled();

		ImGui::TextUnformatted("Output path");
		ImGui::SameLine(offset, padding);
		ImGui::SetNextItemWidth(-btn_pad);
		ImGui::InputText("##out_path", &out_path);
		ImGui::SameLine();
		if (ImGui::Button("Browse...##out_browse")) {
			nfdu8char_t* path = nullptr;
			nfdsavedialogu8args_t args = {0};
			args.filterList = pdf ? pdf_filter : png_filter;
			args.filterCount = 1;
			nfdresult_t res = NFD_SaveDialogU8_With(&path, &args);
			if (res == NFD_OKAY) {
				out_path = path;
				NFD_FreePathU8(path);
			}
		}

		if (ImGui::Button("Load image", ImVec2(ImGui::CalcTextSize("Output path").x, 0.0f)))
			load_image_preview(img, in_path.c_str(), cfg);
		ImGui::SameLine();
		ImGui::BeginDisabled();
		ImGui::Checkbox("Create PDF", &pdf_check);
		ImGui::EndDisabled();
		ImGui::SameLine(ImGui::GetWindowWidth()
			- style.WindowPadding.x - 2 * style.FramePadding.x - ImGui::CalcTextSize("Browse...").x
			- 2 * style.ItemSpacing.x - ImGui::CalcTextSize("Output dimensions").x - 150.0f);
		ImGui::TextUnformatted("Output dimensions");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputInt2("##out_size", &cfg.width);
		ImGui::SameLine();
		ImGui::Button("Process", ImVec2(2 * style.FramePadding.x + ImGui::CalcTextSize("Browse...").x, 0.0f));
		/*
		if (ImGui::CollapsingHeader("PDF creation")) {
			if (!pdf) ImGui::BeginDisabled();
			ImGui::Button("+");
			ImGui::SameLine();
			ImGui::Button("-");
			ImGui::SameLine();
			ImGui::Button("Clear...");
			ImGui::SameLine();
			ImGui::ArrowButton("##up", ImGuiDir_Up);
			ImGui::SameLine();
			ImGui::ArrowButton("##down", ImGuiDir_Down);
			ImGui::SameLine();
			ImGui::Button("Sort...");

			static const ImGuiTableFlags flags =
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_BordersOuter |
				ImGuiTableFlags_ScrollY;
			if (ImGui::BeginTable("files", 5, flags)) {
				std::string* paths = pdf_paths.data();
				for (size_t i = 0, size = pdf_paths.size(); i < size; i++) {
					ImGui::TableNextColumn();
					ImGui::TableNextRow();
				}
				ImGui::EndTable();
			}
			if (!pdf) ImGui::EndDisabled();
		}
		*/
		pdf = pdf_check;
	}
	ImGui::End();
}

static void window_preview(struct image& img, struct config& cfg) {
	static ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::Begin("Preview")) {
		ImGui::TextUnformatted("Preview:");
		int stage = cfg.prev_stage;
		if (ImGui::RadioButton("None", &cfg.prev_stage, PNGSQ_PREVIEW_NONE)) {
			if (img.tex0 != 0 && img.tex0 != img.tex1)
				glDeleteTextures(1, &img.tex0);
			img.tex0 = 0;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Original", &cfg.prev_stage, PNGSQ_PREVIEW_ORIGINAL) && stage != cfg.prev_stage) {
			if (img.tex0 != 0 && img.tex0 != img.tex1)
				glDeleteTextures(1, &img.tex0);
			img.tex0 = 0;
			glGenTextures(1, &img.tex0);
			glBindTexture(GL_TEXTURE_2D, img.tex0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cfg.prev_width, cfg.prev_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data[0]);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Dewarped", &cfg.prev_stage, PNGSQ_PREVIEW_DEWARPED) && stage != cfg.prev_stage) {
			if (img.tex0 != 0 && img.tex0 != img.tex1)
				glDeleteTextures(1, &img.tex0);
			img.tex0 = img.tex1;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Processed", &cfg.prev_stage, PNGSQ_PREVIEW_PROCESSED) && stage != cfg.prev_stage) {
			if (img.tex0 != 0 && img.tex0 != img.tex1)
				glDeleteTextures(1, &img.tex0);
			img.tex0 = 0;
			glGenTextures(1, &img.tex0);
			glBindTexture(GL_TEXTURE_2D, img.tex0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		}
		if (ImGui::CollapsingHeader("Settings")) {
			ImGui::TextUnformatted("Limit preview to:");
			if (ImGui::InputInt("KB", &cfg.prev_kbytes, 1024, 256)) {
				
			}
			// float width = (ImGui::CalcItemWidth() - 2.0f * style.ItemSpacing.x) / 3.0f;
			ImGui::BeginDisabled();
			ImGui::EndDisabled();

			ImGui::TextUnformatted("Number of colours sampled");
			ImGui::InputInt("##prev_sampled", &cfg.prev_sampled, 0);
			ImGui::TextUnformatted("Maximum number of k-means iterations");
			ImGui::InputInt("##prev_iters", &cfg.prev_iters, 0);
			ImGui::TextUnformatted("Update interval");
			ImGui::InputInt("ms", &cfg.prev_interval, 0);
		}
	}
	ImGui::End();
}

static void window_processing(struct image& img, struct config& cfg) {
	if (ImGui::Begin("Processing")) {
		static float palette[45] = { 0.0f };
		ImGui::TextUnformatted("Number of colours sampled");
		ImGui::InputInt("##sampled", &cfg.sampled, 0);
		ImGui::TextUnformatted("Maximum number of k-means iterations");
		ImGui::InputInt("##iters", &cfg.iters, 0);
		static const ImGuiTableFlags flags =
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_ScrollY;
		if (ImGui::CollapsingHeader("Palette", ImGuiTreeNodeFlags_DefaultOpen) && ImGui::BeginTable("palette", 2, flags)) {
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("14").x);
			ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();
			for (int i = 1; i < 16; ++i) {
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%d", i);
				ImGui::TableNextColumn();
				if (cfg.auto_palette) {
					palette[3 * i - 3] = img.palette[i].r / 255.0f;
					palette[3 * i - 2] = img.palette[i].g / 255.0f;
					palette[3 * i - 1] = img.palette[i].b / 255.0f;
				}
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::ColorEdit3("##entry", &palette[3 * i - 3]) && !cfg.auto_palette) {
					img.palette[i].r = (unsigned char)std::roundf(palette[3 * i - 3] * 255.0f);
					img.palette[i].g = (unsigned char)std::roundf(palette[3 * i - 2] * 255.0f);
					img.palette[i].b = (unsigned char)std::roundf(palette[3 * i - 1] * 255.0f);
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}
