#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "head.hpp"
#include "buffer.hpp"

#include "glad/glad.h"
#include "libdeflate/libdeflate.h"

#ifdef _WIN32
#	define STBI_WINDOWS_UTF8
#endif // _WIN32

#include "stb_image.h"
#include "stb_image_resize2.h"

static struct image load_image_internal(char const* path) {
	struct image img = {0};
	int channels = 0;
	stbi_convert_iphone_png_to_rgb(1);
	size_t size = 0;
	if (!stbi_is_16_bit(path)) {
		img.data[0] = stbi_load(path, &img.width, &img.height, &channels, STBI_rgb);
		size = (size_t)3 * img.width * img.height;
	}
	else {
		stbi_us* raw = stbi_load_16(path, &img.width, &img.height, &channels, STBI_rgb);
		if (raw == nullptr)
			return img;
		size = (size_t)3 * img.width * img.height;
		// Use `malloc` so that `free` can be called on `data`
		img.data[0] = (unsigned char*)std::malloc(size);
		if (img.data == nullptr)
			return img;
		for (size_t i = 0; i < size; i++)
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 6386)
#endif // _MSC_VER
			img.data[0][i] = (unsigned char)(raw[i] >> 8);
#ifdef _MSC_VER
#	pragma warning(pop)
#endif // _MSC_VER
		stbi_image_free(raw);
	}
	return img;
}

bool load_image(struct image& img, char const* path) {
	struct image temp = load_image_internal(path);
	if (temp.data == nullptr)
		return false;
	img = temp;
	return true;
}

bool load_image_preview(struct image& img, char const* path, const struct config& cfg) {
	struct image temp = load_image_internal(path);
	if (temp.data == nullptr)
		return false;
	unsigned char* data = stbir_resize_uint8_srgb(
		temp.data[0], temp.width, temp.height, 0,
		nullptr, cfg.prev_width, cfg.prev_height, 0,
		STBIR_RGB);
	stbi_image_free(temp.data);
	if (data == nullptr)
		return false;
	free_image(img);
	img.data[0] = data;
	img.width = cfg.prev_width;
	img.height = cfg.prev_height;
	return true;
}

void free_image(struct image& img) {
	stbi_image_free(img.data[0]);
	stbi_image_free(img.data[1]);
	stbi_image_free(img.data[2]);
	if (img.tex0 != 0) {
		glDeleteTextures(1, &img.tex0);
		img.tex0 = 0;
	}
	if (img.tex1 != 0) {
		glDeleteTextures(1, &img.tex1);
		img.tex1 = 0;
	}
}

#pragma pack(push, 1)
// Functor that makes conversion from `uint32_t` to `char*` a bit cleaner
class u32_to_8 {
	const char arr[4];
public:
	// Converts `uint32_t` to `char*` with correct byte order
	constexpr explicit u32_to_8(const uint32_t& x) :
		arr { (char)(x >> 24), (char)((x >> 16) & 0xff), (char)((x >> 8) & 0xff), (char)(x & 0xff) } { }
	constexpr operator char const* () const { return arr; }
};
#pragma pack(pop)

static inline struct rgb& px_from_coord(const struct image& img, int x, int y, const struct config& cfg) {
	return *bytes_to_rgb(&img.data[2][(y * (size_t)cfg.width + x) * 3]);
}

static void put_scanlines(const struct image& img, buffer& buf, struct libdeflate_compressor* compressor, const struct config& cfg) {
	unsigned char* data = img.data[2];
	struct rgb const* const end = img.palette + sizeof(img.palette) / sizeof(img.palette[0]);
	const bool odd = cfg.width % 2;
	const int count = cfg.width - (int)odd;
	const size_t length = (cfg.width + (size_t)odd) / 2 + 1;
	const size_t bytes = cfg.height * length;
	auto lines = std::make_unique<unsigned char[]>(bytes);
	for (int i = 0; i < cfg.height; i++) {
		lines[i * length] = 0;
		for (int j = 0; j < count; j += 2) {
			unsigned char p1 = (unsigned char)(std::find(img.palette, end, px_from_coord(img, j, i, cfg)) - img.palette);
			unsigned char p2 = (unsigned char)(std::find(img.palette, end, px_from_coord(img, j + 1, i, cfg)) - img.palette);
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 6386)
#endif // _MSC_VER
			lines[j / 2 + i * length + 1] = (p1 << 4) | p2;
#ifdef _MSC_VER
#	pragma warning(pop)
#endif // _MSC_VER
		}
		if (odd) {
			unsigned char p1 = (unsigned char)(std::find(img.palette, end, px_from_coord(img, count, i, cfg)) - img.palette);
			lines[count / 2 + i * length + 1] = p1 << 4;
		}
	}
	buf.alloc(bytes + 1024);
	buf.alloc(libdeflate_zlib_compress(compressor, lines.get(), bytes, buf.data() + 4, buf.size()) + 4);
}

static inline void write_chunk(std::ofstream& out, buffer& buf) {
	if (buf.size() < 4)
		return;
	out.write(u32_to_8((uint32_t)buf.size() - 4), 4);
	out.write(buf.data(), buf.size());
	out.write(u32_to_8(libdeflate_crc32(0, buf.data(), buf.size())), 4);
}

// Get rid of annoying IntelliSense error
#if defined(_WIN32) && defined(_MSC_VER)
extern "C" int __stdcall MultiByteToWideChar(unsigned int cp, unsigned long flags, const char* str, int cbmb, wchar_t* widestr, int cchwide);
#endif // defined(_WIN32) && defined(_MSC_VER)

bool write_image(const struct image& img, char const* path, struct libdeflate_compressor* compressor, const struct config& cfg) {
	buffer buf;
#ifdef _WIN32
	if (std::strlen(path) > INT_MAX)
		return false;
	int len = MultiByteToWideChar(65001, 0, path, (int)std::strlen(path), nullptr, 0);
	auto wide = std::make_unique<wchar_t[]>((size_t)1 + len);
	MultiByteToWideChar(65001, 0, path, (int)std::strlen(path), wide.get(), len);
	std::ofstream out(std::filesystem::path(std::wstring(wide.get())), std::ios::out | std::ios::trunc | std::ios::binary);
#else
	std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
#endif
	out.write("\x89\x50\x4e\x47\x0d\x0a\x1a\x0a", 8);
	// IHDR
	buf.alloc(17);
	buf.put("\x49\x48\x44\x52", 4);
	buf.put(u32_to_8(cfg.width), 4);
	buf.put(u32_to_8(cfg.height), 4);
	buf.put("\4\3\0\0\0", 5); // Bit depth 4, colour type 3, compression method 0, filter method 0, interlace method 0
	write_chunk(out, buf);
	buf.dealloc();
	// PLTE
	buf.alloc(52);
	buf.put("\x50\x4c\x54\x45", 4);
	for (int i = 0; i < 16; i++)
		buf.put(reinterpret_cast<char const*>(&img.palette[i]), 3);
	write_chunk(out, buf);
	buf.dealloc();
	// IDAT
	buf.alloc(4);
	buf.put("\x49\x44\x41\x54", 4);
	put_scanlines(img, buf, compressor, cfg);
	write_chunk(out, buf);
	buf.dealloc();
	// IEND
	buf.alloc(4);
	buf.put("\x49\x45\x4e\x44", 4);
	write_chunk(out, buf);
	buf.dealloc();
	out.flush();
	out.close();
	return !out.fail();
}
