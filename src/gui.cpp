#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "nativefiledialog-extended/src/include/nfd.h"

#include "head.hpp"
#include "gui.hpp"
#include "perspective.hpp"

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
static void window_file(struct image& img, struct wndinfo& wnd, struct config& cfg);\
static void window_processing(struct image& img, struct config& cfg);
static void window_settings(struct config& cfg);

void draw(struct image& img, struct wndinfo& wnd) {
	static struct config cfg = {
		.auto_palette = true,
		.prev_kbytes = 20000
	};
	static std::vector<struct threshold> thresholds;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	window_settings(cfg);
	window_background(img, thresholds, cfg);
	window_processing(img, cfg);
	window_preview(img, wnd, cfg);
	window_file(img, wnd, cfg);

	ImGui::ShowStyleEditor();
	ImGui::Render();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, wnd.width, wnd.height);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static constexpr ImGuiTableFlags table_flags =
	ImGuiTableFlags_RowBg |
	ImGuiTableFlags_BordersOuter |
	ImGuiTableFlags_ScrollY;

static void window_background(struct image& img, std::vector<struct threshold>& thresholds, struct config& cfg) {
	static ImGuiStyle& style = ImGui::GetStyle();

	if (ImGui::Begin("Background")) {
		static float colours[6] = { 0.0f };
		colours[0] = std::roundf(cfg.ovr_bg_before_col.r / 2.55f) / 100.0f;
		colours[1] = std::roundf(cfg.ovr_bg_before_col.g / 2.55f) / 100.0f;
		colours[2] = std::roundf(cfg.ovr_bg_before_col.b / 2.55f) / 100.0f;
		colours[3] = std::roundf(cfg.ovr_bg_after_col.r  / 2.55f) / 100.0f;
		colours[4] = std::roundf(cfg.ovr_bg_after_col.g  / 2.55f) / 100.0f;
		colours[5] = std::roundf(cfg.ovr_bg_after_col.b  / 2.55f) / 100.0f;

		ImGui::Checkbox("Override background before processing", &cfg.ovr_bg_before);
		ImGui::SameLine();
		ImGui::BeginDisabled(!cfg.ovr_bg_before);
		if (ImGui::ColorEdit3("##before", &colours[0], ImGuiColorEditFlags_NoInputs)) {
			cfg.ovr_bg_before_col.r = (unsigned char)std::roundf(colours[0] * 255.0f);
			cfg.ovr_bg_before_col.g = (unsigned char)std::roundf(colours[1] * 255.0f);
			cfg.ovr_bg_before_col.b = (unsigned char)std::roundf(colours[2] * 255.0f);
		}
		ImGui::EndDisabled();

		ImGui::Checkbox("Override background after processing", &cfg.ovr_bg_after);
		ImGui::SameLine(0.0f, style.ItemSpacing.x + ImGui::CalcTextSize("before").x - ImGui::CalcTextSize("after").x);
		ImGui::BeginDisabled(!cfg.ovr_bg_after);
		if (ImGui::ColorEdit3("##after", &colours[3], ImGuiColorEditFlags_NoInputs)) {
			cfg.ovr_bg_after_col.r = (unsigned char)std::roundf(colours[3] * 255.0f);
			cfg.ovr_bg_after_col.g = (unsigned char)std::roundf(colours[4] * 255.0f);
			cfg.ovr_bg_after_col.b = (unsigned char)std::roundf(colours[5] * 255.0f);
		}
		ImGui::EndDisabled();

		ImGui::Checkbox("Dark mode", &cfg.dark);
		ImGui::SameLine();
		Tooltip("(?)", "Use this if the background of the image is darker than the text\n(e.g. text on a blackboard)");

		if (ImGui::Button("+")) {
			static constexpr threshold thr_default = {
				.selected = false,
				.diff = { 0.0f, 0.0f, 0.0f },
				.mode = PNGSQ_VAL_MODE_RANGE,
				.enabled = true
			};
			thresholds.push_back(thr_default);
		}
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
		Tooltip("(?)",
			"The Mode column changes how pngsquish interprets the Value slider.\n"
			"Suppose the Value slider is set to V and the background has value B.\n"
			"- Range (default): All pixels with value within B\u00B1V (and within hue/saturation ranges) are considered part of the background.\n"
			"- Compare: More aggressive; all pixels with value greater than B-V (for dark mode: value less than B+V) are considered part of the background.\n"
			"The Hue and Saturation sliders always behave under Range mode.");

		if (ImGui::BeginTable("thresholds", 6, table_flags)) {
			float btn_width = ImGui::CalcTextSize("Compare").x + 2 * style.FramePadding.x;
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
				if (ImGui::BeginItemTooltip()) {
					ImGui::TextUnformatted("Click to toggle, hover the (?) above for more info");
					ImGui::EndTooltip();
				}
				ImGui::TableNextColumn();
				ImGui::Checkbox("##enabled", &thrs[i].enabled);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

static void window_file(struct image& img, struct wndinfo& wnd, struct config& cfg) {
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
	
	if (ImGui::Begin("File")) {
		float offset = ImGui::CalcTextSize("Output path").x;
		float padding = style.WindowPadding.x + style.ItemSpacing.x;
		float btn_pad = style.WindowPadding.x + 2 * style.FramePadding.x + ImGui::CalcTextSize("Browse...").x;

		ImGui::BeginDisabled(wnd.pdf_mode);
		ImGui::TextUnformatted("Input path");
		ImGui::SameLine(offset, padding);
		ImGui::SetNextItemWidth(-btn_pad);
		ImGui::InputText("##in_path", &in_path);
		ImGui::SameLine();
		ImGui::BeginDisabled(!wnd.nfd_init);
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
		ImGui::EndDisabled();
		if (!wnd.pdf_mode && !wnd.nfd_init && ImGui::BeginItemTooltip()) {
			if (NFD_GetError() != nullptr)
				ImGui::Text("NFD error: %s", NFD_GetError());
			else
				ImGui::TextUnformatted("NFD error");
			ImGui::EndTooltip();
		}
		ImGui::EndDisabled();

		ImGui::TextUnformatted("Output path");
		ImGui::SameLine(offset, padding);
		ImGui::SetNextItemWidth(-btn_pad);
		ImGui::InputText("##out_path", &out_path);
		ImGui::SameLine();
		ImGui::BeginDisabled(!wnd.nfd_init);
		if (ImGui::Button("Browse...##out_browse")) {
			nfdu8char_t* path = nullptr;
			nfdsavedialogu8args_t args = {0};
			args.filterList = wnd.pdf_mode ? pdf_filter : png_filter;
			args.filterCount = 1;
			nfdresult_t res = NFD_SaveDialogU8_With(&path, &args);
			if (res == NFD_OKAY) {
				out_path = path;
				NFD_FreePathU8(path);
			}
		}
		if (!wnd.nfd_init && ImGui::BeginItemTooltip()) {
			if (NFD_GetError() != nullptr)
				ImGui::Text("NFD error: %s", NFD_GetError());
			else
				ImGui::TextUnformatted("NFD error");
			ImGui::EndTooltip();
		}
		ImGui::EndDisabled();

		if (ImGui::Button("Load image", ImVec2(ImGui::CalcTextSize("Output path").x, 0.0f)))
			load_image_preview(img, in_path.c_str(), cfg);
		ImGui::SameLine();
		ImGui::BeginDisabled();
		ImGui::Checkbox("Create PDF", &pdf);
		ImGui::EndDisabled();
		ImGui::SameLine(ImGui::GetWindowWidth()
			- 2 * style.WindowPadding.x - 2 * style.FramePadding.x - 2 * style.ItemSpacing.x - 100.0f * wnd.scale
			- ImGui::CalcTextSize("Browse...").x - ImGui::CalcTextSize("Output dimensions").x - ImGui::CalcTextSize("(!)").x);
		ImGui::TextUnformatted("Output dimensions");
		ImGui::SameLine();
		Tooltip("(!)", "Tip: Set this to 0 to match input dimensions");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f * wnd.scale);
		ImGui::InputInt2("##out_size", &cfg.width);
		ImGui::SameLine();
		ImGui::Button("Process", ImVec2(2 * style.FramePadding.x + ImGui::CalcTextSize("Browse...").x, 0.0f));

		/*
		if (ImGui::CollapsingHeader("PDF creation")) {
			ImGui::BeginDisabled(!wnd.pdf_mode);
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
			ImGui::EndDisabled();
		}
		*/
		wnd.pdf_mode = pdf;
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

		if (ImGui::CollapsingHeader("Palette", ImGuiTreeNodeFlags_DefaultOpen) && ImGui::BeginTable("palette", 2, table_flags)) {
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
				ImGui::BeginDisabled(cfg.auto_palette);
				if (ImGui::ColorEdit3("##entry", &palette[3 * i - 3])) {
					img.palette[i].r = (unsigned char)std::roundf(palette[3 * i - 3] * 255.0f);
					img.palette[i].g = (unsigned char)std::roundf(palette[3 * i - 2] * 255.0f);
					img.palette[i].b = (unsigned char)std::roundf(palette[3 * i - 1] * 255.0f);
				}
				ImGui::EndDisabled();
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

static void window_settings(struct config& cfg) {
	if (ImGui::Begin("Settings")) {
		{
			static char const* const options[] = {
				"Add vertex, or move nearest vertex if 4 vertices have been added",
				"Add vertex, or replace oldest vertex if 4 vertices have been added",
				"Add vertex, and clear all previous vertices if 4 have been added"
			};
			ImGui::TextUnformatted("Image preview click behaviour (for dewarping)");
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::Combo("##click", &cfg.prev_click_behaviour, options, sizeof(options) / sizeof(options[0]));
		}
	}
	ImGui::End();
}
