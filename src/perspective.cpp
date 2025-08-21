#include <cfloat>
#include <cstdio>
#include <cstring>
#include <deque>

#include "glad/glad.h"

#include "head.hpp"
#include "matrix.hpp"
#include "perspective.hpp"

namespace {
	GLuint vao, vbo, program, fbo;
	char const* shader_vert = PNGSQ_GLSL_VERSION_STRING R"(
in vec4 vertex;
out vec2 coords;
uniform mat3 transform;

void main() {
	vec3 transformed = transform * vec3(vertex.xy, 1.0);
	gl_Position = vec4(transformed.xy / transformed.z, 1.0, 1.0);
	coords = vertex.zw;
}
)""\0";
	char const* shader_frag = PNGSQ_GLSL_VERSION_STRING R"(
in vec2 coords;
out vec4 outcol;
uniform sampler2D sampler;

void main() {
	outcol = texture(sampler, coords);
}
)""\0";
}

bool init_shaders_persp(void) {
	static constexpr GLfloat rect[] = {
		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f, 0.0f,
		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 1.0f, 0.0f
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
	glBindAttribLocation(::program, 0, "vertex");
	glAttachShader(::program, vert);
	glAttachShader(::program, frag);
	glLinkProgram(::program);
	glDeleteShader(vert);
	glDeleteShader(frag);
	glUseProgram(::program);
	glUniform1i(glGetUniformLocation(::program, "sampler"), 0);

	glGenVertexArrays(1, &::vao);
	glGenBuffers(1, &::vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ::vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);
	glBindVertexArray(::vao);
	glEnableVertexAttribArray(glGetAttribLocation(::program, "vertex"));
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glGenFramebuffers(1, &::fbo);
	return true;
}

void deinit_shaders_persp(void) {
	if (::program != 0) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &::fbo);
		glUseProgram(0);
		glDeleteProgram(::program);
		glDeleteVertexArrays(1, &::vao);
		glDeleteBuffers(1, &::vbo);
		program = 0;
	}
}

// Scales a point from normalized coordinates to image coordinates
static constexpr struct point scale(const struct point& p, const struct image& img) {
	return { p.x * img.width, p.y * img.height };
}

// Computes the perspective transformation matrix from quadrilateral `img.dewarp_src` to a 2x2 square centred at (0, 0)
// Based on Heckbert (1989), page 20
mat<3> persp_matrix(const struct image& img) {
	struct point p0 = scale(img.dewarp_src.p[0], img);
	struct point p1 = scale(img.dewarp_src.p[1], img);
	struct point p2 = scale(img.dewarp_src.p[2], img);
	struct point p3 = scale(img.dewarp_src.p[3], img);
	float dx1 = p1.x - p2.x;
	float dx2 = p3.x - p2.x;
	float sx  = p0.x - p1.x - dx2;
	float dy1 = p1.y - p2.y;
	float dy2 = p3.y - p2.y;
	float sy  = p0.y - p1.y - dy2;
	float d   = (dx1 * dy2 - dy1 * dx2);
	float m31 = (sx * dy2 - sy * dx2) / d;
	float m32 = (dx1 * sy - dy1 * sx) / d;
	mat<3> m;
	m(1, 1) = p1.x - p0.x + m31 * p1.x;
	m(1, 2) = p3.x - p0.x + m32 * p3.x;
	m(1, 3) = p0.x;
	m(2, 1) = p1.y - p0.y + m31 * p1.y;
	m(2, 2) = p3.y - p0.y + m32 * p3.y;
	m(2, 3) = p0.y;
	m(3, 1) = m31;
	m(3, 2) = m32;
	m(3, 3) = 1.0f;
	return mat<3>( // transform to NDC
		2.0f,  0.0f, -1.0f,
		0.0f,  2.0f, -1.0f,
		0.0f,  0.0f,  1.0f
	) * ~m;
}

void transform_image(struct image& img, const mat<3>& transform, const struct config& cfg) {
	std::free(img.data_dewarp);
	std::free(img.data_output);
	size_t size = (size_t)3 * img.out_width * img.out_height;
	img.data_dewarp = (unsigned char*)std::malloc(size);
	img.data_output = (unsigned char*)std::malloc(size);
	if (img.data_dewarp == nullptr || img.data_output == nullptr)
		return;
	glBindFramebuffer(GL_FRAMEBUFFER, ::fbo);
	glViewport(0, 0, img.out_width, img.out_height);
	GLuint tex_dewarp = create_texture(nullptr, img.out_width, img.out_height, false);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_dewarp, 0);
	glUseProgram(::program);
	glUniformMatrix3fv(glGetUniformLocation(::program, "transform"), 1, GL_TRUE, transform.ptr());
	glActiveTexture(GL_TEXTURE0);
	GLuint tex_original = cfg.prev_stage == PNGSQ_PREVIEW_ORIGINAL ? img.texture : create_texture(img.data_orig, img.width, img.height, false);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindVertexArray(::vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, img.out_width, img.out_height, GL_RGB, GL_UNSIGNED_BYTE, img.data_dewarp);
	std::memcpy(img.data_output, img.data_dewarp, size);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (cfg.prev_stage != PNGSQ_PREVIEW_ORIGINAL)
		glDeleteTextures(1, &tex_original);
	if (cfg.prev_stage != PNGSQ_PREVIEW_DEWARPED)
		glDeleteTextures(1, &tex_dewarp);
	else if (img.texture != 0) {
		glDeleteTextures(1, &img.texture);
		img.texture = tex_dewarp;
	}
}
