/*
 * Copyright (c) 2008 James Molloy, Jörg Pfähler, Matthew Iselin
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef KERNEL_UTILITY_H
#define KERNEL_UTILITY_H

#include <stdarg.h>
#include <processor/types.h>

/** @addtogroup kernelutilities
 * @{ */

#ifdef __cplusplus
extern "C" {
#endif

// Endianness shizzle.
#define BS8(x) (x)
#define BS16(x) (((x&0xFF00)>>8)|((x&0x00FF)<<8))
#define BS32(x) (((x&0xFF000000)>>24)|((x&0x00FF0000)>>8)|((x&0x0000FF00)<<8)|((x&0x000000FF)<<24))
#define BS64(x) (x)

#ifdef LITTLE_ENDIAN

#define LITTLE_TO_HOST8(x) (x)
#define LITTLE_TO_HOST16(x) (x)
#define LITTLE_TO_HOST32(x) (x)
#define LITTLE_TO_HOST64(x) (x)

#define HOST_TO_LITTLE8(x) (x)
#define HOST_TO_LITTLE16(x) (x)
#define HOST_TO_LITTLE32(x) (x)
#define HOST_TO_LITTLE64(x) (x)

#define BIG_TO_HOST8(x) BS8((x))
#define BIG_TO_HOST16(x) BS16((x))
#define BIG_TO_HOST32(x) BS32((x))
#define BIG_TO_HOST64(x) BS64((x))

#define HOST_TO_BIG8(x) BS8((x))
#define HOST_TO_BIG16(x) BS16((x))
#define HOST_TO_BIG32(x) BS32((x))
#define HOST_TO_BIG64(x) BS64((x))

#else // else Big endian

#define BIG_TO_HOST8(x) (x)
#define BIG_TO_HOST16(x) (x)
#define BIG_TO_HOST32(x) (x)
#define BIG_TO_HOST64(x) (x)

#define HOST_TO_BIG8(x) (x)
#define HOST_TO_BIG16(x) (x)
#define HOST_TO_BIG32(x) (x)
#define HOST_TO_BIG64(x) (x)

#define LITTLE_TO_HOST8(x) BS8((x))
#define LITTLE_TO_HOST16(x) BS16((x))
#define LITTLE_TO_HOST32(x) BS32((x))
#define LITTLE_TO_HOST64(x) BS64((x))

#define HOST_TO_LITTLE8(x) BS8((x))
#define HOST_TO_LITTLE16(x) BS16((x))
#define HOST_TO_LITTLE32(x) BS32((x))
#define HOST_TO_LITTLE64(x) BS64((x))

#endif

int vsprintf(char *buf, const char *fmt, va_list arg);
int sprintf(char *buf, const char *fmt, ...);
size_t strlen(const char *buf);
int strcpy(char *dest, const char *src);
int strncpy(char *dest, const char *src, int len);
void *memset(void *buf, int c, size_t len);
void *wmemset(void *buf, int c, size_t len);
void *dmemset(void *buf, unsigned int c, size_t len);
void *qmemset(void *buf, unsigned long long c, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
void *memmove(void *s1, const void *s2, size_t n);
int memcmp(const void *p1, const void *p2, size_t len);

int strcmp(const char *p1, const char *p2);
int strncmp(const char *p1, const char *p2, int n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, int n);

const char *strchr(const char *str, int target);
const char *strrchr(const char *str, int target);

unsigned long strtoul(const char *nptr, char **endptr, int base);

inline char toUpper(char c)
{
    if (c < 'a' || c > 'z')
        return c; // special chars
    c += ('A' - 'a');
    return c;
}

inline char toLower(char c)
{
    if (c < 'A' || c > 'Z')
        return c; // special chars
    c -= ('A' - 'a');
    return c;
}

#ifdef __cplusplus
}
#endif

#define MAX_FUNCTION_NAME 128
#define MAX_PARAMS 32
#define MAX_PARAM_LENGTH 64

#ifdef __cplusplus
  /** Add a offset (in bytes) to the pointer and return the result
   *\brief Adjust a pointer
   *\return new pointer pointing to 'pointer + offset' (NOT pointer arithmetic!) */
  template<typename T>
  inline T *adjust_pointer(T *pointer, size_t offset)
  {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(pointer) + offset);
  }

  template<typename T>
  inline void swap(T a, T b)
  {
      T t = a;
      a = b;
      b = t;
  }

  inline uint8_t checksum(const uint8_t *pMemory, size_t sMemory)
  {
    uint8_t sum = 0;
    for (size_t i = 0;i < sMemory;i++)
      sum += reinterpret_cast<const uint8_t*>(pMemory)[i];
    return (sum == 0);
  }

  template<typename T>
  struct is_integral
  {
    static const bool value = false;
  };
  template<>struct is_integral<bool>{static const bool value = true;};
  template<>struct is_integral<char>{static const bool value = true;};
  template<>struct is_integral<unsigned char>{static const bool value = true;};
  template<>struct is_integral<signed char>{static const bool value = true;};
  template<>struct is_integral<short>{static const bool value = true;};
  template<>struct is_integral<unsigned short>{static const bool value = true;};
  template<>struct is_integral<int>{static const bool value = true;};
  template<>struct is_integral<unsigned int>{static const bool value = true;};
  template<>struct is_integral<long>{static const bool value = true;};
  template<>struct is_integral<unsigned long>{static const bool value = true;};
  template<>struct is_integral<long long>{static const bool value = true;};
  template<>struct is_integral<unsigned long long>{static const bool value = true;};

  template<typename T>
  struct is_pointer
  {
    static const bool value = false;
  };
  template<typename T>
  struct is_pointer<T*>
  {
    static const bool value = true;
  };
#endif

/** @} */

#endif
