#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <filesystem>
#endif

#define STB_IMAGE_IMPLEMENTATION

#include "head.hpp"
#include "buffer.hpp"

img_t load_image(char const* path) {
	img_t image{};
	int channels = 0;
	stbi_convert_iphone_png_to_rgb(1);
	if (!stbi_is_16_bit(path))
		image.data = stbi_load(path, &image.width, &image.height, &channels, STBI_rgb);
	else {
		stbi_us* data = stbi_load_16(path, &image.width, &image.height, &channels, STBI_rgb);
		if (!data)
			return image;
		size_t size = (size_t)image.width * (size_t)image.height * (size_t)channels;
		// Use `malloc` so that `stbi_image_free` can be called on `image.data`
		image.data = (stbi_uc*)malloc(size * sizeof(stbi_uc));
		for (size_t i = 0; i < size; i++)
			image.data[i] = (stbi_uc)(data[i] >> 8);
		stbi_image_free(data);
	}
	return image;
}

#pragma pack(push, 1)
class u32_to_8 {
	char a, b, c, d;
public:
	constexpr explicit u32_to_8(const uint32_t x) : a(x >> 24), b((x >> 16) & 0xff), c((x >> 8) & 0xff), d(x & 0xff) { }
	constexpr operator char* () const { return (char*)this; }
};
#pragma pack(pop)

static constexpr rgb_t* px_from_coord(const img_t& image, int x, int y) {
	return (rgb_t*)&image.data[(y * (size_t)image.width + x) * 3];
}

static inline void put_scanlines(const img_t& image, buffer& buf, libdeflate_compressor* compressor) {
	rgb_t const* end = image.palette + sizeof(image.palette) / sizeof(image.palette[0]);
	const bool odd = image.width % 2;
	const int count = image.width - (int)odd;
	const size_t length = (image.width + (size_t)odd) / 2 + 1;
	size_t bytes = image.height * length;
	unsigned char* lines = new unsigned char[bytes];
	for (int i = 0; i < image.height; i++) {
		lines[i * length] = 0;
		for (int j = 0; j < count; j += 2) {
			unsigned char p1 = (unsigned char)(std::find(image.palette, end, *px_from_coord(image, j, i)) - image.palette);
			unsigned char p2 = (unsigned char)(std::find(image.palette, end, *px_from_coord(image, j + 1, i)) - image.palette);
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 6386)
#endif // _MSC_VER
			lines[j / 2 + i * length + 1] = (p1 << 4) | p2;
#ifdef _MSC_VER
#	pragma warning(pop)
#endif
		}
		if (odd) {
			unsigned char p1 = (unsigned char)(std::find(image.palette, end, *px_from_coord(image, count, i)) - image.palette);
			lines[count / 2 + i * length + 1] = p1 << 4;
		}
	}
	buf.alloc(bytes + 4);
	buf.alloc(libdeflate_zlib_compress(compressor, lines, bytes, buf.data() + 4, buf.size()) + 4);
}

static inline void write_chunk(std::ofstream& out, buffer& buf) {
	if (buf.size() < 4)
		return;
	out.write(u32_to_8((uint32_t)buf.size() - 4), 4);
	out.write(buf.data(), buf.size());
	out.write(u32_to_8(libdeflate_crc32(0, buf.data(), buf.size())), 4);
}

bool write_image(const img_t& image, char const* path, libdeflate_compressor* compressor) {
	buffer buf;
#ifdef _WIN32
	if (std::strlen(path) > INT_MAX)
		return false;
	int len = MultiByteToWideChar(65001, 0, path, (int)std::strlen(path), nullptr, 0);
	wchar_t* wide = new wchar_t[len + 1];
	MultiByteToWideChar(65001, 0, path, (int)std::strlen(path), wide, len);
	wide[len] = (wchar_t)0;
	std::ofstream out(std::filesystem::path(std::wstring(wide)), std::ios::out | std::ios::trunc | std::ios::binary);
#else
	std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
#endif
	out.write("\x89\x50\x4e\x47\x0d\x0a\x1a\x0a", 8);
	// IHDR
	buf.alloc(17);
	buf.put("\x49\x48\x44\x52", 4);
	buf.put(u32_to_8(image.width), 4);
	buf.put(u32_to_8(image.height), 4);
	buf.put("\4\3\0\0\0", 5); // Bit depth 4, colour type 3, compression method 0, filter method 0, interlace method 0
	write_chunk(out, buf);
	buf.dealloc();
	// PLTE
	buf.alloc(52);
	buf.put("\x50\x4c\x54\x45", 4);
	for (int i = 0; i < 16; i++)
		buf.put(reinterpret_cast<char const*>(&image.palette[i]), 3);
	write_chunk(out, buf);
	buf.dealloc();
	// IDAT
	buf.alloc(4);
	buf.put("\x49\x44\x41\x54", 4);
	put_scanlines(image, buf, compressor);
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
