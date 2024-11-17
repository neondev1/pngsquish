#ifndef PNGSQ_BUFFER_HPP
#define PNGSQ_BUFFER_HPP

#include <cstring>

class buffer {
protected:
	char* buf;
	size_t len, pos;
public:
	inline buffer(void);
	inline buffer(size_t len);
	inline ~buffer();

	inline size_t put(char const* data, size_t count);
	inline size_t put(char data);

	inline void alloc(size_t count);
	inline void dealloc(void);

	constexpr char* data(void);
	constexpr size_t size(void) const;
	constexpr char* offset(size_t count);
};

inline buffer::buffer(void) : buf(nullptr), len(0), pos(0) { }

inline buffer::buffer(size_t len) : len(len), pos(0) {
	this->buf = new char[len];
}

inline buffer::~buffer() {
	if (this->buf)
		delete[] this->buf;
}

inline size_t buffer::put(char const* data, size_t count) {
	if (!this->buf)
		return 0;
	const size_t bytes = std::min(this->len - this->pos, count);
	std::memcpy(this->buf + this->pos, data, bytes);
	this->pos += bytes;
	return bytes;
}

inline size_t buffer::put(char data) {
	if (!this->buf || this->pos == this->len)
		return 0;
	buf[this->pos++] = data;
	return 1;
}

inline void buffer::alloc(size_t count) {
	char* nbuf = new char[count];
	if (this->buf) {
		std::memcpy(nbuf, this->buf, std::min(this->len, count));
		this->pos = std::min(this->pos, count);
	}
	else
		this->pos = 0;
	delete[] this->buf;
	this->buf = nbuf;
	this->len = count;
	this->pos = 0;
}

inline void buffer::dealloc(void) {
	if (this->buf)
		delete[] this->buf;
	this->buf = nullptr;
	this->len = 0;
	this->pos = 0;
}

constexpr char* buffer::data(void) {
	return this->buf;
}

constexpr size_t buffer::size(void) const {
	return this->len;
}

constexpr char* buffer::offset(size_t count) {
	return this->buf + (this->pos = count);
}

#endif
