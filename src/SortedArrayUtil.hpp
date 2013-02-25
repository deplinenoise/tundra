#ifndef SORTEDARRAYUTIL_HPP
#define SORTEDARRAYUTIL_HPP

#include <string.h>
#include "Common.hpp"

namespace t2
{

template <
  typename KeySelect1, typename KeySelect2, typename ArrayCallback1, typename ArrayCallback2>
void TraverseSortedArrays(
    const size_t      size1,
    ArrayCallback1    callback1,
    KeySelect1        key_select1,
    const size_t      size2,
    ArrayCallback2    callback2,
    KeySelect2        key_select2)
{
  typedef decltype(key_select1(0)) KeyT;

#if ENABLED(CHECKED_BUILD)
  // Paranoia to check both arrays are sorted.
  for (size_t i = 1; i < size1; ++i)
  {
    const HashDigest* a = key_select1(i - 1);
    const HashDigest* b = key_select1(i);
    CHECK(*a < *b);
  }
  for (size_t i = 1; i < size2; ++i)
  {
    const HashDigest* a = key_select2(i - 1);
    const HashDigest* b = key_select2(i);
    CHECK(*a < *b);
  }
#endif

  size_t i1 = 0, i2 = 0;

  while (i1 < size1 && i2 < size2)
  {
    const HashDigest* record_1 = key_select1(i1);
    const HashDigest* record_2 = key_select2(i2);

    int compare = CompareHashDigests(*record_1, *record_2);

    if (compare <= 0)
    {
      callback1(i1);
      ++i1;

      if (0 == compare)
      {
        ++i2;
      }
    }
    else
    {
      callback2(i2);
      ++i2;
    }
  }

  while (i1 < size1)
  {
    callback1(i1);
    ++i1;
  }

  while (i2 < size2)
  {
    callback2(i2);
    ++i2;
  }
}

}

#endif
