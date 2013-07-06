#ifndef PKGDEPDB_ENDIAN_H__
#define PKGDEPDB_ENDIAN_H__

#include <type_traits>

template<typename T> inline T Eswap_be(T x);

template<> inline uint8_t  Eswap_be(uint8_t  x) {
	return x;
}

template<> inline uint16_t Eswap_be(uint16_t x) {
	uint8_t *y = (uint8_t*)&x;
	return (uint16_t(y[0])<<(1*8)) |
	       (uint16_t(y[1])<<(0*8));
}

template<> inline uint32_t Eswap_be(uint32_t x) {
	uint8_t *y = (uint8_t*)&x;
	return (uint32_t(y[0])<<(3*8)) |
	       (uint32_t(y[1])<<(2*8)) |
	       (uint32_t(y[2])<<(1*8)) |
	       (uint32_t(y[3])<<(0*8));
}

template<> inline uint64_t Eswap_be(uint64_t x) {
	uint8_t *y = (uint8_t*)&x;
	return (uint64_t(y[0])<<(7*8)) |
	       (uint64_t(y[1])<<(6*8)) |
	       (uint64_t(y[2])<<(5*8)) |
	       (uint64_t(y[3])<<(4*8)) |
	       (uint64_t(y[4])<<(3*8)) |
	       (uint64_t(y[5])<<(2*8)) |
	       (uint64_t(y[6])<<(1*8)) |
	       (uint64_t(y[7])<<(0*8));
}

template<typename T> inline T Eswap_le(T x);

template<> inline uint8_t  Eswap_le(uint8_t  x) {
	return x;
}

template<> inline uint16_t Eswap_le(uint16_t x) {
	uint8_t *y = (uint8_t*)&x;
	return (uint16_t(y[0])<<(0*8)) |
	       (uint16_t(y[1])<<(1*8));
}

template<> inline uint32_t Eswap_le(uint32_t x) {
	uint8_t *y = (uint8_t*)&x;
	return (uint32_t(y[0])<<(0*8)) |
	       (uint32_t(y[1])<<(1*8)) |
	       (uint32_t(y[2])<<(2*8)) |
	       (uint32_t(y[3])<<(3*8));
}

template<> inline uint64_t Eswap_le(uint64_t x) {
	uint8_t *y = (uint8_t*)&x;
	return (uint64_t(y[0])<<(0*8)) |
	       (uint64_t(y[1])<<(1*8)) |
	       (uint64_t(y[2])<<(2*8)) |
	       (uint64_t(y[3])<<(3*8)) |
	       (uint64_t(y[4])<<(4*8)) |
	       (uint64_t(y[5])<<(5*8)) |
	       (uint64_t(y[6])<<(6*8)) |
	       (uint64_t(y[7])<<(7*8));
}

template<bool BE, typename T> inline T Eswap(T x) {
	if (BE)
		return static_cast<T>(Eswap_be<typename std::make_unsigned<T>::type>(x));
	else
		return static_cast<T>(Eswap_le<typename std::make_unsigned<T>::type>(x));
}

#endif
