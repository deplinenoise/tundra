#ifndef SORTEDARRAYUTIL_HPP
#define SORTEDARRAYUTIL_HPP

#include "Common.hpp"
#include "Hash.hpp"

#include <algorithm>

namespace t2
{

inline int CompareArrayKeys(const HashDigest* l, const HashDigest* r)
{
  return CompareHashDigests(*l, *r);
}

template <
  typename KeySelect1,
  typename KeySelect2,
  typename ArrayCallback1,
  typename ArrayCallback2,
  typename KeyType1,
  typename KeyType2>
void TraverseSortedArraysImpl(
    const size_t      size1,
    ArrayCallback1    callback1,
    KeySelect1        key_select1,
    const size_t      size2,
    ArrayCallback2    callback2,
    KeySelect2        key_select2)
{
  size_t i1 = 0, i2 = 0;

  while (i1 < size1 && i2 < size2)
  {
    const KeyType1 record_1 = key_select1(i1);
    const KeyType2 record_2 = key_select2(i2);

    int compare = CompareArrayKeys(record_1, record_2);

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
  TraverseSortedArraysImpl
    <KeySelect1, KeySelect2, ArrayCallback1, ArrayCallback2, decltype(key_select1(0)), decltype(key_select2(0))>
    (size1, callback1, key_select1, size2, callback2, key_select2);
}

template <typename T>
const T* BinarySearch(const T* table, int count, const T& key)
{
  if (count > 0)
  {
    const T* end = table + count;
    const T* iter = std::lower_bound(table, end, key);
    if (iter != end && *iter == key)
    {
      return iter;
    }
  }
  return nullptr;
}

}

#endif
