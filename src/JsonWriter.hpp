#ifndef JSONWRITER_HPP
#define JSONWRITER_HPP

#include "Buffer.hpp"

namespace t2
{

struct JsonWriter
{
  MemAllocHeap* m_Heap;
  Buffer<char> m_Buffer;
  bool m_PrependComma;
};

void JsonWriteInit(JsonWriter* writer, MemAllocHeap* heap);
void JsonWriteDestroy(JsonWriter* writer);

void JsonWriteReset(JsonWriter* writer);

void JsonWriteStartObject(JsonWriter* writer);
void JsonWriteEndObject(JsonWriter* writer);

void JsonWriteStartArray(JsonWriter* writer);
void JsonWriteEndArray(JsonWriter* writer);


void JsonWriteKeyName(JsonWriter* writer, const char* keyName);

void JsonWriteValueString(JsonWriter* writer, const char* value);
void JsonWriteValueInteger(JsonWriter* writer, int64_t value);

}

#endif