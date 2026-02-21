#include "string.h"

size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    if (n == (size_t)-1) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *kstrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *kstrncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

void *kmemset(void *dst, int val, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = (uint8_t)val;
    return dst;
}

void *kmemcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

static const char digits[] = "0123456789ABCDEF";

void kutoa(uint64_t val, char *buf, int base) {
    char tmp[65];
    int  i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (val) {
        tmp[i++] = digits[val % (uint64_t)base];
        val /= (uint64_t)base;
    }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
}

void kitoa(int64_t val, char *buf, int base) {
    if (base == 10 && val < 0) {
        *buf++ = '-';
        kutoa((uint64_t)(-val), buf, base);
    } else {
        kutoa((uint64_t)val, buf, base);
    }
}
