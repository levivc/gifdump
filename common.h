#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "config.h"

#if defined(_MSC_VER)
  #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
  #define FORCE_INLINE __attribute__((always_inline)) static inline
#else
  #define FORCE_INLINE static inline
#endif

#ifdef _WIN64
  typedef long long s_size;
  #define S_SIZE_MIN LLONG_MIN
  #define S_SIZE_MAX LLONG_MAX
#else
  typedef long s_size;
  #define S_SIZE_MIN LONG_MIN
  #define S_SIZE_MAX LONG_MAX
#endif

#define swap(a, b, size) do                                                   \
{                                                                             \
    char buffer[64];                                                          \
    size_t buffer_size = sizeof(buffer);                                      \
    size_t lval_size = (size);                                                \
    char* ap = (char*)(a);                                                    \
    char* bp = (char*)(b);                                                    \
    while (lval_size)                                                         \
    {                                                                         \
        if (lval_size < buffer_size)                                          \
            buffer_size = lval_size;                                          \
        memcpy(buffer, ap, buffer_size);                                      \
        memmove(ap, bp, buffer_size);                                         \
        memcpy(bp, buffer, buffer_size);                                      \
        ap += buffer_size;                                                    \
        bp += buffer_size;                                                    \
        lval_size -= buffer_size;                                             \
    }                                                                         \
} while (0)

#define swap_bytes(p, size) do {                                              \
    char byte = 0;                                                            \
    char* front = (char*)(p);                                                 \
    char* back = front + (size) - 1;                                          \
    while (front < back)                                                      \
    {                                                                         \
        byte = *front;                                                        \
        *front = *back;                                                       \
        *back = byte;                                                         \
        ++front;                                                              \
        --back;                                                               \
    }                                                                         \
} while (0);

FORCE_INLINE uint16_t swap_bytes16(uint16_t v)
{
    return (v << 8) | (v >> 8);
}

FORCE_INLINE uint32_t swap_bytes32(uint32_t v)
{
    return (v << 24) | ((v << 8) & 0x00FF0000) | ((v >> 8) & 0x0000FF00) | (v >> 24);
}

FORCE_INLINE uint64_t swap_bytes64(uint64_t v)
{
    return (v << 56)                        | ((v << 40) & 0x00FF000000000000) |
           ((v << 24) & 0x0000FF0000000000) | ((v << 8)  & 0x000000FF00000000) |
           ((v >> 8) & 0x00000000FF000000)  | ((v >> 24) & 0x0000000000FF0000) |
           ((v >> 40) & 0x000000000000FF00) | (v >> 56);
}

#endif
