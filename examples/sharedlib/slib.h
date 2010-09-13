#ifndef SLIB_H
#define SLIB_H

#ifdef _MSC_VER
#ifdef SLIB_BUILDING
#define SLIB_API __declspec(dllexport)
#else
#define SLIB_API __declspec(dllimport)
#endif
#else
#define SLIB_API __attribute__((visibility("default")))
#endif

SLIB_API void slib_func(void);

#endif
