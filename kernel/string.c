#include <stddef.h>
#include <stdint.h>
#include "types.h"

void* memset(void *dst, int c, uint n) {
    char *cdst = (char *) dst;
    int i;
    for(i = 0; i < n; i++){
        cdst[i] = c;
    }
    return dst;
}

int memcmp(const void *v1, const void *v2, uint n) {
    const uchar *s1, *s2;
    s1 = v1;
    s2 = v2;
    while(n-- > 0){
        if(*s1 != *s2)
            return *s1 - *s2;
        s1++, s2++;
    }
    return 0;
}

void* memmove(void *dst, const void *src, uint n) {
    const char *s;
    char *d;

    s = src;
    d = dst;
    if(s < d && s + n > d){
        s += n;
        d += n;
        while(n-- > 0)
            *--d = *--s;
    } else {
        while(n-- > 0)
            *d++ = *s++;
    }
    return dst;
}

// 复制字符串 src 到 dst，最多复制 n 个字符
char* strncpy(char *s, const char *t, int n) {
    char *os = s;
    while(n-- > 0 && (*s++ = *t++) != 0)
        ;
    while(n-- > 0)
        *s++ = 0;
    return os;
}

int strlen(const char *s) {
    int n;
    for(n = 0; s[n]; n++)
        ;
    return n;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}