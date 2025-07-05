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

	size_t put(char const* data, size_t count);
	size_t put(char data);

	void alloc(size_t count);
	void dealloc(void);

	constexpr char* data(void);
	constexpr size_t size(void) const;
	constexpr char* offset(size_t count);
};

inline buffer::buffer(void) : buf(nullptr), len(0), pos(0) { }

inline buffer::buffer(size_t len) : len(len), pos(0) {
	if (len != 0) this->buf = new char[len];
}

inline buffer::~buffer() {
	delete[] this->buf;
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
