#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <string>

#include "glad/glad.h"
#include "libdeflate/libdeflate.h"

#include "head.hpp"
#include "buffer.hpp"

#ifdef _WIN32
#	define STBI_WINDOWS_UTF8
#endif // _WIN32

#include "stb_image.h"
#include "stb_image_resize2.h"

static struct image load_image_internal(char const* path) {
	struct image img = {
		.dewarp_src = { invalid_point }
	};
	int channels = 0;
	stbi_convert_iphone_png_to_rgb(1);
	if (!stbi_is_16_bit(path))
		img.data_orig = stbi_load(path, &img.full_width, &img.full_height, &channels, STBI_rgb);
	else {
		stbi_us* raw = stbi_load_16(path, &img.full_width, &img.full_height, &channels, STBI_rgb);
		if (raw == nullptr)
			return img;
		size_t size = (size_t)3 * img.full_width * img.full_height;
		// Use `malloc` so that `free` can be called on `data`
		img.data_orig = (unsigned char*)std::malloc(size);
		if (img.data_orig == nullptr)
			return img;
		for (size_t i = 0; i < size; i++)
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 6386)
#endif // _MSC_VER
			img.data_orig[i] = (unsigned char)(raw[i] >> 8);
#ifdef _MSC_VER
#	pragma warning(pop)
#endif // _MSC_VER
		std::free(raw);
	}
	return img;
}

bool load_image(struct image& img, char const* path) {
	struct image temp = load_image_internal(path);
	if (temp.data_orig == nullptr)
		return false;
	img = temp;
	img.width = temp.full_width;
	img.height = temp.full_height;
	return true;
}

bool load_image_preview(struct image& img, char const* path, const struct config& cfg) {
	struct image temp = load_image_internal(path);
	if (temp.data_orig == nullptr)
		return false;
	temp.path = (char*)std::malloc(std::strlen(path) + 1);
	if (temp.path == nullptr) {
		free_image(temp);
		return false;
	}
	std::strcpy(temp.path, path);
	temp.dewarp_src = img.dewarp_src;
	float scale = calc_prev_scale(temp.full_width, temp.full_height, cfg.prev_kbytes);
	if (scale >= 1.0f) {
		img = temp;
		img.width = img.full_width;
		img.height = img.full_height;
		return true;
	}
	temp.width = scale * temp.full_width;
	temp.height = scale * temp.full_height;
	unsigned char* data = stbir_resize_uint8_srgb(
		temp.data_orig, temp.full_width, temp.full_height, 0,
		nullptr, temp.width, temp.height, 0,
		STBIR_RGB);
	std::free(temp.data_orig);
	if (data == nullptr)
		return false;
	free_image(img);
	img = temp;
	img.data_orig = data;
	if (cfg.prev_stage == PNGSQ_PREVIEW_ORIGINAL)
		img.texture = create_texture(data, img.width, img.height, false);
	return true;
}

void free_image(struct image& img) {
	std::free(img.data_orig);
	std::free(img.data_dewarp);
	std::free(img.data_output);
	if (img.texture != 0) {
		glDeleteTextures(1, &img.texture);
		img.texture = 0;
	}
}

static inline struct rgb& px_from_coord(const struct image& img, int x, int y) {
	return *bytes_to_rgb(&img.data_output[(y * (size_t)img.out_width + x) * 3]);
}

static void put_scanlines(const struct image& img, buffer& buf, struct libdeflate_compressor* compressor) {
	unsigned char* data = img.data_output;
	struct rgb const* const end = img.palette + sizeof(img.palette) / sizeof(img.palette[0]);
	const bool odd = img.out_width % 2;
	const int count = img.out_width - (int)odd;
	const size_t length = (img.out_width + (size_t)odd) / 2 + 1;
	const size_t bytes = img.out_height * length;
	auto lines = std::make_unique<unsigned char[]>(bytes);
	for (int line = 0; line < img.out_height; line++) {
		lines[line * length] = 0;
		for (int i = 0; i < count; i += 2) {
			unsigned char p1 = (unsigned char)(std::find(img.palette, end, px_from_coord(img, i, line)) - img.palette);
			unsigned char p2 = (unsigned char)(std::find(img.palette, end, px_from_coord(img, i + 1, line)) - img.palette);
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 6386)
#endif // _MSC_VER
			lines[i / 2 + line * length + 1] = (p1 << 4) | p2;
#ifdef _MSC_VER
#	pragma warning(pop)
#endif // _MSC_VER
		}
		if (odd) {
			unsigned char p1 = (unsigned char)(std::find(img.palette, end, px_from_coord(img, count, line)) - img.palette);
			lines[count / 2 + line * length + 1] = p1 << 4;
		}
	}
	size_t max_size = bytes + 5 * ((size_t)std::floor(bytes / 16383.0) + 1) + 6;
	buf.alloc(max_size + 4);
	buf.alloc(libdeflate_zlib_compress(compressor, lines.get(), bytes, buf.data() + 4, buf.size()) + 4); // Shrink the buffer
}

// For correct byte ordering
#define U32_TO_8(x) { (char)((x) >> 24), (char)(((x) >> 16) & 0xff), (char)(((x) >> 8) & 0xff), (char)((x) & 0xff) }

static inline void write_chunk(std::ofstream& out, const buffer& buf) {
	if (buf.size() < 4)
		return;
	uint32_t size = (uint32_t)buf.size() - 4, crc = libdeflate_crc32(0, buf.data(), buf.size());
	char size_bytes[4] = U32_TO_8(size);
	char crc_bytes[4] = U32_TO_8(crc);
	out.write(size_bytes, 4);
	out.write(buf.data(), buf.size());
	out.write(crc_bytes, 4);
}

// Get rid of annoying IntelliSense error
#if defined(_WIN32) && defined(_MSC_VER)
extern "C" int __stdcall MultiByteToWideChar(unsigned int cp, unsigned long flags, const char* str, int cbmb, wchar_t* widestr, int cchwide);
#endif // defined(_WIN32) && defined(_MSC_VER)

bool write_image(const struct image& img, char const* path, struct libdeflate_compressor* compressor) {
	buffer buf;
#ifdef _WIN32
	if (std::strlen(path) > INT_MAX)
		return false;
	constexpr unsigned utf8 = 65001u;
	int len = MultiByteToWideChar(utf8, 0, path, (int)std::strlen(path), nullptr, 0);
	auto wide = std::make_unique<wchar_t[]>((size_t)1 + len);
	MultiByteToWideChar(utf8, 0, path, (int)std::strlen(path), wide.get(), len);
	std::ofstream out(std::filesystem::path(std::wstring(wide.get())), std::ios::out | std::ios::trunc | std::ios::binary);
#else
	std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
#endif
	out.write("\x89\x50\x4e\x47\x0d\x0a\x1a\x0a", 8);
	try {
		// IHDR
		buf.alloc(17);
		buf.put("\x49\x48\x44\x52", 4);
		buf.put((uint32_t)img.out_width);
		buf.put((uint32_t)img.out_height);
		buf.put("\4\3\0\0\0", 5); // Bit depth 4, colour type 3, compression method 0, filter method 0, interlace method 0
		write_chunk(out, buf);
		buf.free();
		// PLTE
		buf.alloc(52);
		buf.put("\x50\x4c\x54\x45", 4);
		for (int i = 0; i < 16; i++)
			buf.put(reinterpret_cast<char const*>(&img.palette[i]), 3);
		write_chunk(out, buf);
		buf.free();
		// IDAT
		buf.alloc(4);
		buf.put("\x49\x44\x41\x54", 4);
		put_scanlines(img, buf, compressor);
		write_chunk(out, buf);
		buf.free();
		// IEND
		buf.alloc(4);
		buf.put("\x49\x45\x4e\x44", 4);
		write_chunk(out, buf);
		buf.free();
	}
	catch (std::bad_alloc& e) {
		return false;
	}
	out.flush();
	out.close();
	return !out.fail();
}
