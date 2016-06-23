#pragma once

// Use this recursive fibonacci template by Bruce Dawson to create a header
// file that is very slow to compile.
// https://randomascii.wordpress.com/2014/03/10/making-compiles-slow/

template <int TreePos, int N>
struct FibSlow_t {
    enum { value = FibSlow_t<TreePos, N - 1>::value + FibSlow_t<TreePos + (1 << N), N - 2>::value, };
};

// Explicitly specialized for N==2
template <int TreePos>
struct FibSlow_t<TreePos, 2> {
	enum { value = 1 };
};

// Explicitly specialized for N==1
template <int TreePos>
struct FibSlow_t<TreePos, 1> {
	enum { value = 1 };
};

// Be CAREFUL with high values. 24 is pretty slow on GCC 4.8. Larger amounts will use a ton of RAM
inline int SlowDown() { return FibSlow_t<0,20>::value; }

#include <stdio.h>
#include "included_in_pch.h"
