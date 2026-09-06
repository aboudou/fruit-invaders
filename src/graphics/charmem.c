#include "charmem.h"

void charmem_load(unsigned char first_code, const unsigned char *src, unsigned char count) {
    volatile unsigned char *dst = CHARSET_BASE + (unsigned)first_code * 8;
    while (count--) {
        *dst++ = (unsigned char)~(*src++);
    }
}
