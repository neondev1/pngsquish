#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <deque>
#include <string>

#include "glad/glad.h"

#include "head.hpp"
#include "gui.hpp"

namespace {
	GLuint vao, vbo, program, fbo, texture;
	char const* shader_vert = PNGSQ_GLSL_VERSION_STRING R"(
in vec2 vertex;
uniform mat3 transform[8];

void main() {
	gl_Position = vec4(transform[gl_InstanceID] * vec3(vertex, 1.0), 1.0);
}
)""\0";
	char const* shader_frag = PNGSQ_GLSL_VERSION_STRING R"(
out vec4 outcol;
uniform vec4 colour;

void main() {
	outcol = colour;
}
)""\0";
}

static constexpr float dist2(const struct point& left, const struct point& right);
static bool fix_quad(struct quad* out, const std::deque<struct point>& in);
static void draw_vertex(struct point vert, ImVec2 prev_pos, int id);
static void draw_edge(struct point v1, struct point v2, ImVec2 prev_pos, int id);

void window_preview(struct image& img, struct wndinfo& wnd, struct config& cfg) {
	static ImGuiStyle& style = ImGui::GetStyle();

	if (ImGui::Begin("Preview")) {
		ImGui::TextUnformatted("Preview:");
		int stage = cfg.prev_stage;
		if (ImGui::RadioButton("None", &cfg.prev_stage, PNGSQ_PREVIEW_NONE) && stage != cfg.prev_stage) {
			if (img.texture != 0)
				glDeleteTextures(1, &img.texture);
			img.texture = 0;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Original", &cfg.prev_stage, PNGSQ_PREVIEW_ORIGINAL) && stage != cfg.prev_stage) {
			if (img.texture != 0)
				glDeleteTextures(1, &img.texture);
			img.texture = create_texture(img.data_orig, img.width, img.height, false);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Dewarped", &cfg.prev_stage, PNGSQ_PREVIEW_DEWARPED) && stage != cfg.prev_stage) {

		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Processed", &cfg.prev_stage, PNGSQ_PREVIEW_PROCESSED) && stage != cfg.prev_stage) {
			if (img.texture != 0)
				glDeleteTextures(1, &img.texture);
			img.texture = create_texture(img.data_output, img.out_width, img.out_height, false);
		}

		if (img.texture != 0) {
			float prev_width = ImGui::GetWindowWidth() - 2 * style.WindowPadding.x;
			float prev_height = prev_width * (cfg.prev_stage == PNGSQ_PREVIEW_ORIGINAL ?
				(float)img.full_height / (float)img.full_width : (float)img.out_height / (float)img.out_width);
			ImVec2 prev_pos = ImGui::GetCursorScreenPos();
			ImVec2 prev_size = ImVec2(prev_width, prev_height);
			ImGui::Image(img.texture, prev_size);
			if (cfg.prev_stage == PNGSQ_PREVIEW_ORIGINAL) {
				static std::deque<struct point> vertices;
				static bool valid_quad;

				ImGui::SetCursorScreenPos(prev_pos);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				bool clicked = ImGui::ImageButton("preview", ::texture, prev_size);
				ImGui::PopStyleColor(3);
				ImGui::PopStyleVar();

				if (clicked) {
					if (img.dewarp_src.p[0] == invalid_point)
						vertices.clear();

					ImVec2 mouse_pos = ImGui::GetMousePos();
					struct point p = { (mouse_pos.x - prev_pos.x) / prev_width, 1.0f - (mouse_pos.y - prev_pos.y) / prev_height };

					if (vertices.size() < 4 && vertices.end() == std::find(vertices.begin(), vertices.end(), p)) {
						vertices.push_back(p);
						img.dewarp_src.p[vertices.size() - 1] = p;
						if (vertices.size() == 4)
							valid_quad = fix_quad(&img.dewarp_src, vertices);
					}
					else if (p == vertices.front() || vertices.end() == std::find(vertices.begin(), vertices.end(), p)) {
						switch (cfg.prev_click_behaviour) {
						case PNGSQ_VERT_MOVE_NEAREST:
						{
							auto vert = vertices.begin();
							float best = FLT_MAX;
							for (auto it = vertices.begin(); it != vertices.end(); ++it) {
								float dist = dist2(p, *it);
								if (best > dist) {
									best = dist;
									vert = it;
								}
							}
							vertices.erase(vert);
							vertices.push_back(p);
							valid_quad = fix_quad(&img.dewarp_src, vertices);
							break;
						}
						case PNGSQ_VERT_MOVE_OLDEST:
							vertices.pop_front();
							vertices.push_back(p);
							valid_quad = fix_quad(&img.dewarp_src, vertices);
							break;
						case PNGSQ_VERT_CLEAR_ALL:
							vertices.clear();
							vertices.push_back(p);
							break;
						}
					}
				}

				if (clicked || wnd.size_changed && !vertices.empty()) {
					static struct point last;
					GLsizei draw_count;
					switch (vertices.size()) {
					case 1:
						last = vertices.back();
						draw_vertex(last, prev_size, 0);
						draw_count = 1;
						break;
					case 2:
					case 3:
						draw_edge(vertices.back(), last, prev_size, 2 * vertices.size() - 3);
						last = vertices.back();
						draw_vertex(last, prev_size, 2 * vertices.size() - 2);
						draw_count = 2 * vertices.size() - 1;
						break;
					case 4:
						for (int i = 0; i < 4; i++) {
							draw_vertex(img.dewarp_src.p[i], prev_size, 2 * i);
							draw_edge(img.dewarp_src.p[i], img.dewarp_src.p[(i + 1) % 4], prev_size, 2 * i + 1);
						}
						draw_count = 8;
						break;
					}

					glUseProgram(::program);
					glUniform4fv(glGetUniformLocation(::program, "colour"), 1, &ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg).x);
					glBindFramebuffer(GL_FRAMEBUFFER, ::fbo);
					glViewport(0, 0, prev_width, prev_height);
					if (::texture != 0)
						glDeleteTextures(1, &::texture);
					::texture = create_texture(nullptr, prev_width, prev_height, true);
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ::texture, 0);
					glUseProgram(::program);
					glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
					glClear(GL_COLOR_BUFFER_BIT);
					glBindVertexArray(::vao);
					glEnable(GL_MULTISAMPLE);
					glDrawArraysInstanced(GL_TRIANGLES, 0, 6, draw_count);
					glDisable(GL_MULTISAMPLE);
					glBindVertexArray(0);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					wnd.size_changed = false;
				}

				if (vertices.size() < 4)
					ImGui::TextUnformatted("Click on the image to select the corners of the page");
				else if (valid_quad) {
					switch (cfg.prev_click_behaviour) {
					case PNGSQ_VERT_MOVE_NEAREST:
						ImGui::TextUnformatted("Corners selected; click image to move the nearest vertex to the cursor");
						break;
					case PNGSQ_VERT_MOVE_OLDEST:
						ImGui::TextUnformatted("Corners selected; click image to replace the oldest vertex");
						break;
					case PNGSQ_VERT_CLEAR_ALL:
						ImGui::TextUnformatted("Corners selected; click image to draw new corners");
						break;
					}
					if (ImGui::BeginItemTooltip()) {
						ImGui::TextUnformatted("Behaviour can be changed in settings");
						ImGui::EndTooltip();
					}
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
					ImGui::TextUnformatted("Error: Quadrilateral cannot be degenerate or concave");
					ImGui::PopStyleColor();
				}
			}
		}

		if (ImGui::CollapsingHeader("Settings")) {
			ImGui::TextUnformatted("Limit preview to:");
			ImGui::InputInt("KB", &cfg.prev_kbytes, 8192, 1024);
			if (ImGui::IsItemDeactivatedAfterEdit() && img.path != nullptr)
				load_image_preview(img, img.path, cfg);

			int orig_percent = img.full_width != 0 ? (int)std::roundf(100.0f * img.width / img.full_width) : 0;
			int out_percent = img.full_width != 0 ? (int)std::roundf(100.0f * img.out_width / (cfg.width == 0 ? img.full_width : cfg.width)) : 0;
			float box_width = (ImGui::CalcItemWidth() - style.ItemSpacing.x - 2 * style.ItemInnerSpacing.x - ImGui::CalcTextSize("%").x) / 3.0f;

			ImGui::TextUnformatted("Preview dimensions:");
			ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(std::roundf(box_width));
			ImGui::InputInt("%##original", &orig_percent, 0);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(std::roundf(2 * box_width + style.ItemInnerSpacing.x));
			ImGui::InputInt2("Original", &img.width);
			ImGui::SetNextItemWidth(std::roundf(box_width));
			ImGui::InputInt("%##output", &out_percent, 0);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(std::roundf(2 * box_width + style.ItemInnerSpacing.x));
			ImGui::InputInt2("Output", &img.out_width);
			ImGui::EndDisabled();

			ImGui::TextUnformatted("Update interval");
			ImGui::SameLine();
			Tooltip("(?)", "Interval at which the preview is updated.\nSet to 0 to disable auto-updating.");
			ImGui::InputInt("ms", &cfg.prev_interval, 0);
		}
	}
	ImGui::End();
}

static constexpr float dist2(const struct point& left, const struct point& right) {
	const float dx = left.x - right.x, dy = left.y - right.y;
	return dx * dx + dy * dy;
}

// Copies points from the deque `in` to the quadrilateral `out`.
// Ensures that vertices are specified in counterclockwise order starting from the bottom-left corner.
// Fixes self-intersecting quadrilaterals and rejects concave/degenerate quadrilaterals.
// Returns true on success, false on error (if the quadrilateral is concave/degenerate)
static bool fix_quad(struct quad* out, const std::deque<struct point>& in) {
	struct vec2 { float x, y; };
	if (in.size() != 4)
		return false;
	std::deque<struct point> copy = in;
	float products[4] = { 0.0f };
	int positive_count = 0;

	bool degenerate = false;
	for (size_t i = 0; i < 4; i++) {
		out->p[i] = in[i];
		struct point& a = copy[(i + 3) % 4]; // last vertex
		struct point& b = copy[i];           // current vertex
		struct point& c = copy[(i + 1) % 4]; // next vertex
		struct vec2 ab = { b.x - a.x, b.y - a.y };
		struct vec2 bc = { c.x - b.x, c.y - b.y };
		/*
		if (ab.x == 0 && ab.y == 0 || bc.x == 0 && bc.y == 0)
			degenerate = true;
		*/
		// Compute cross product of vectors AB and BC; we are interested in the sign
		products[i] = ab.x * bc.y - ab.y * bc.x;
		if (products[i] == 0.0f)
			degenerate = true;
		else if (products[i] > 0.0f)
			positive_count++;
	}

	if (degenerate)
		return false;

	bool err = false;
	switch (positive_count) {
	case 0: // Vertices are in clockwise order, need to reverse them
		for (size_t i = 0; i < 4; i++)
			copy[i] = in[3 - i];
		break;
	case 1: // Concave quadrilateral
		err = true;
		break;
	case 2: // Self-intersecting quadrilateral, swap vertices with negative cross products to fix
	{
		size_t vert = 0;
		for (size_t i = 0; i < 4; i++)
			if (products[vert = i] < 0.0f && products[(i + 1) % 4] < 0.0f)
				break;
		copy[vert] = in[(vert + 1) % 4];
		copy[(vert + 1) % 4] = in[vert];
		break;
	}
	case 3: // Concave quadrilateral
		err = true;
		break;
	case 4: // Vertices are already in counterclockwise order
		break;
	}

	struct point const* first = nullptr;
	if (err && first != nullptr) {
		// Maintain ordering of points on error, otherwise stuff looks weird
		for (int i = 0; i < 4; i++) {
			struct point const* p = &copy.front();
			if (p == first)
				break;
			copy.push_back(*p);
			copy.pop_front();
		}
	}
	else {
		float best = FLT_MAX; // Closest Euclidean distance to bottom left
		for (size_t i = 0; i < 4; i++) {
			float dist = in[i].x + in[i].y;
			if (best > dist)
				best = dist;
		}
		// Shift bottom left point to front of deque
		for (struct point const* p = &copy.front(); p->x + p->y > best; p = &copy.front()) {
			copy.push_back(*p);
			copy.pop_front();
		}
		for (size_t i = 0; i < 4; i++)
			out->p[i] = copy[i];
		first = out->p;
	}
	return !err;
}

static void draw_vertex(struct point v, ImVec2 prev_size, int id) {
	constexpr float size = 10.0f;
	float w = size / prev_size.x;
	float h = size / prev_size.y;
	float x = v.x * 2.0f - 1.0f;
	float y = 1.0f - v.y * 2.0f;
	GLfloat transform[] = {
		   w, 0.0f,    x,
		0.0f,    h,    y,
		0.0f, 0.0f, 1.0f
	};
	
	std::string name = "transform[" + std::to_string(id) + "]";
	glUseProgram(::program);
	glUniformMatrix3fv(glGetUniformLocation(::program, name.c_str()), 1, GL_TRUE, transform);
}

static void draw_edge(struct point v1, struct point v2, ImVec2 prev_size, int id) {
	constexpr float thickness = 6.0f;
	float dx = (v1.x - v2.x) * prev_size.x;
	float dy = (v1.y - v2.y) * prev_size.y;
	// scale
	float w = std::sqrt(dx * dx + dy * dy) / 2.0f;
	float h = thickness / 2.0f;
	// rotate
	float a = std::atanf(-dy / dx);
	float c = std::cos(a), s = std::sin(a);
	// scale to NDC
	float fb_w = prev_size.x / 2.0f;
	float fb_h = prev_size.y / 2.0f;
	// translate
	float x = (v1.x + v2.x) - 1.0f;
	float y = 1.0f - (v1.y + v2.y);
	// Apply scaling (for rectangle size) -> rotation -> scaling (to NDC) -> translation
	// (T * S * R * S * vector)
	// Let W = 1/fb_w, H = 1/fb_h for the sake of clarity here
	// / 1 0 x \ / W 0 0 \ / c -s  0 \ / w 0 0 \   / cwW -shW   x  \
	// | 0 1 y | | 0 H 0 | | s  c  0 | | 0 h 0 | = | swH  chH   y  |
	// \ 0 0 1 / \ 0 0 1 / \ 0  0  1 / \ 0 0 1 /   \  0    0    1  /
	GLfloat transform[] = {
		c * w / fb_w, -s * h / fb_w,  x,
		s * w / fb_h,  c * h / fb_h,  y,
		0.0f,          0.0f,          1.0f
	};

	std::string name = "transform[" + std::to_string(id) + "]";
	glUseProgram(::program);
	glUniformMatrix3fv(glGetUniformLocation(::program, name.c_str()), 1, GL_TRUE, transform);
}

bool init_shaders_prev(void) {
	static constexpr GLfloat rect[] = {
		-1.0f,  1.0f,
		 1.0f, -1.0f,
		-1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f,
		 1.0f, -1.0f,
	};

	::program = 0;
	GLint compiled = 0;
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &::shader_vert, nullptr);
	glCompileShader(vert);
	glGetShaderiv(vert, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint len = 0;
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &len);
		try {
			GLchar* err = new GLchar[len];
			glGetShaderInfoLog(vert, len, &len, err);
			std::fprintf(stderr, PNGSQ_ERROR_STRING "Failed to compile vertex shader: %s\n", err);
			delete[] err;
		}
		catch (std::bad_alloc& e) {
			std::fprintf(stderr, PNGSQ_ERROR_STRING "Failed to compile vertex shader\n");
			std::fprintf(stderr, PNGSQ_ERROR_STRING "Out of memory? %s\n", e.what());
		}
		glDeleteShader(vert);
		return false;
	}
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &::shader_frag, nullptr);
	glCompileShader(frag);
	glGetShaderiv(frag, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint len = 0;
		glGetShaderiv(frag, GL_INFO_LOG_LENGTH, &len);
		try {
			GLchar* err = new GLchar[len];
			glGetShaderInfoLog(frag, len, &len, err);
			std::fprintf(stderr, PNGSQ_ERROR_STRING "Failed to compile fragment shader: %s\n", err);
			delete[] err;
		}
		catch (std::bad_alloc& e) {
			std::fprintf(stderr, PNGSQ_ERROR_STRING "Failed to compile fragment shader\n");
			std::fprintf(stderr, PNGSQ_ERROR_STRING "Out of memory? %s\n", e.what());
		}
		glDeleteShader(vert);
		glDeleteShader(frag);
		return false;
	}
	::program = glCreateProgram();
	glAttachShader(::program, vert);
	glAttachShader(::program, frag);
	glBindAttribLocation(::program, 0, "vertex");
	glLinkProgram(::program);
	glDeleteShader(vert);
	glDeleteShader(frag);

	glGenVertexArrays(1, &::vao);
	glGenBuffers(1, &::vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ::vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);
	glBindVertexArray(::vao);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glGenFramebuffers(1, &::fbo);
	glGenTextures(1, &::texture);
	glBindTexture(GL_TEXTURE_2D, ::texture);
	unsigned char zero[] = { 0, 0, 0, 0 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero);
	return true;
}

void deinit_shaders_prev(void) {
	if (::program != 0) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &::fbo);
		glUseProgram(0);
		glDeleteProgram(::program);
		glDeleteVertexArrays(1, &::vao);
		glDeleteBuffers(1, &::vbo);
	}
}
