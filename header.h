#pragma once

// SETTINGS
#define APP_NAME "Rome"
#define DEBUG_VULKAN 0
#define DEBUG_APP 0
#define DEBUG_CPU 0

// VULKAN
#include "shaders.c"
#define USE_DISCRETE_GPU 0
#define ENABLE_HDR 0

// LIBC
extern void exit(int);
extern int printf(const char*,...); // todo: just don't use
extern int snprintf(char*,__SIZE_TYPE__,const char*,...); // todo: just don't use
extern void *memcpy(void *__restrict,const void*__restrict,__SIZE_TYPE__);
extern int memcmp(const void*,const void*,__SIZE_TYPE__);
extern void *memset(void*,int,__SIZE_TYPE__);
extern int strcmp(const char*,const char*);
extern char *strdup(const char*);
extern char *strstr(const char*,const char*);
extern __SIZE_TYPE__ strlen(const char*);
extern void qsort(void *base,__SIZE_TYPE__ nmemb,__SIZE_TYPE__ size,int(*compar)(const void*, const void*));

// MATH
extern double cos(double); extern double sin(double); extern double tan(double); extern double floor(double);
#define cosf cos
#define sinf sin
#define tanf tan
#define floorf floor
#ifdef _WIN32
extern long lround(double) __asm__("_o_lround"); extern long lrint(double) __asm__("_o_lrint");
#else
extern long lround(double); extern long lrint(double); extern double round(double);
#endif
