#include "perspective.hpp"

#include <cstdio>

#include "glad/glad.h"

#include "head.hpp"
#include "matrix.hpp"

namespace {
	GLuint vao, vbo, program, fbo;
	char const* shader_vert = PNGSQ_GLSL_VERSION_STRING R"(
in vec4 vertex;
out vec2 coords;
uniform mat3 transform;

void main() {
	coords = vertex.zw;
	gl_Position = vec4(transform * vec3(vertex.xy, 1.0), 1.0);
}
)""\0";
	char const* shader_frag = PNGSQ_GLSL_VERSION_STRING R"(
in vec2 coords;
out vec4 colour;
uniform sampler2D sampler;

void main() {
	colour = texture(sampler, coords);
}
)""\0";
}

bool init_shaders(void) {
	static const GLfloat quad[] = {
		0.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 0.0f
	};
	program = glCreateProgram();
	GLint compiled = 0;
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &shader_vert, nullptr);
	glCompileShader(vert);
	glGetShaderiv(vert, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint len = 0;
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &len);
		GLchar* err = new GLchar[len];
		glGetShaderInfoLog(vert, len, &len, err);
		std::fprintf(stderr, "Failed to compile vertex shader: %s", err);
		delete[] err;
		glDeleteShader(vert);
		return false;
	}
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &shader_frag, nullptr);
	glCompileShader(frag);
	glGetShaderiv(vert, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint len = 0;
		glGetShaderiv(frag, GL_INFO_LOG_LENGTH, &len);
		GLchar* err = new GLchar[len];
		glGetShaderInfoLog(frag, len, &len, err);
		std::fprintf(stderr, "Failed to compile fragment shader: %s", err);
		delete[] err;
		glDeleteShader(vert);
		glDeleteShader(frag);
		glDeleteProgram(program);
		return false;
	}
	glAttachShader(program, vert);
	glAttachShader(program, frag);
	glLinkProgram(program);
	glDeleteShader(vert);
	glDeleteShader(frag);
	vao = 0;
	vbo = 0;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	fbo = 0;
	glGenFramebuffers(1, &fbo);
	return true;
}

void deinit_shaders(void) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	glUseProgram(0);
	glDeleteProgram(program);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
}

// Computes the perspective transformation matrix from quadrilateral `src` to a 2x2 square centred at (0, 0)
// Based on Heckbert (1989), page 20
mat<3> persp_matrix(const struct quad& src) {
	const float dx1 = src.p1.x - src.p2.x;
	const float dx2 = src.p3.x - src.p2.x;
	const float sx  = src.p0.x - src.p1.x - dx2;
	const float dy1 = src.p1.y - src.p2.y;
	const float dy2 = src.p3.y - src.p2.y;
	const float sy  = src.p0.y - src.p1.y - dy2;
	const float d   = (dx1 * dy2 - dy1 * dx2);
	const float m31 = (sx * dy2 - sy * dx2) / d;
	const float m32 = (dx1 * sy - dy1 * sx) / d;
	mat<3> m;
	m(1, 1) = src.p1.x - src.p0.x + m31 * src.p1.x;
	m(1, 2) = src.p3.x - src.p0.x + m32 * src.p3.x;
	m(1, 3) = src.p0.x;
	m(2, 1) = src.p1.y - src.p0.y + m31 * src.p1.y;
	m(2, 2) = src.p3.y - src.p0.y + m32 * src.p3.y;
	m(2, 3) = src.p0.y;
	m(3, 1) = m31;
	m(3, 2) = m32;
	m(3, 3) = 1.0f;
	return ~m * mat<3>(
		2.0f,  0.0f, -1.0f,
		0.0f,  2.0f, -1.0f,
		0.0f,  0.0f,  1.0f
	);
}

void transform_image(struct image& img, const mat<3>& matrix, const struct config& cfg) {
	if (img.tex1 != 0)
		glDeleteTextures(1, &img.tex1);
	std::free(img.data[1]);
	std::free(img.data[2]);
	img.data[1] = (unsigned char*)std::malloc((size_t)3 * cfg.width * cfg.height);
	img.data[2] = (unsigned char*)std::malloc((size_t)3 * cfg.width * cfg.height);
	if (img.data[1] == nullptr || img.data[2] == nullptr)
		return;
	glViewport(0, 0, cfg.width, cfg.height);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenTextures(1, &img.tex1);
	glBindTexture(GL_TEXTURE_2D, img.tex1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cfg.width, cfg.height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, img.tex1, 0);
	glUseProgram(program);
	glUniformMatrix3fv(glGetUniformLocation(program, "transform"), 1, GL_TRUE, matrix.ptr());
	glUniform1i(glGetUniformLocation(program, "sampler"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, img.tex0);
	glClearColor(img.palette[0].r / 255.0f, img.palette[0].g / 255.0f, img.palette[0].b / 255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glReadPixels(0, 0, cfg.width, cfg.height, GL_RGB, GL_UNSIGNED_BYTE, img.data[1]);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
