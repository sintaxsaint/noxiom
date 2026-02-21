#pragma once
#include <stdint.h>
#include <stddef.h>

size_t   kstrlen(const char *s);
int      kstrcmp(const char *a, const char *b);
int      kstrncmp(const char *a, const char *b, size_t n);
char    *kstrcpy(char *dst, const char *src);
char    *kstrncpy(char *dst, const char *src, size_t n);
void    *kmemset(void *dst, int val, size_t n);
void    *kmemcpy(void *dst, const void *src, size_t n);
void     kitoa(int64_t val, char *buf, int base);
void     kutoa(uint64_t val, char *buf, int base);
