/* WASI 用 lodepng C API ラッパー。C++ リンケージ問題を回避する。 */
#include "lodepng.h"

unsigned ora_lodepng_encode32(unsigned char** out, size_t* outsize, const unsigned char* image, unsigned w, unsigned h) {
  return lodepng_encode32(out, outsize, image, w, h);
}

unsigned ora_lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h, const unsigned char* in, size_t insize) {
  return lodepng_decode32(out, w, h, in, insize);
}

const char* ora_lodepng_error_text(unsigned code) {
  return lodepng_error_text(code);
}
