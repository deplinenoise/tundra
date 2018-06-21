#ifndef JSONWRITER_HPP
#define JSONWRITER_HPP

#include <stdint.h>
#include <stdio.h>

namespace t2
{

struct MemAllocLinear;
struct JsonBlock;

struct JsonWriter
{
  MemAllocLinear* m_Scratch;
  JsonBlock* m_Head;
  JsonBlock* m_Tail;
  uint8_t*   m_Write;
  bool m_PrependComma;
  uint64_t   m_TotalSize;
};

void JsonWriteInit(JsonWriter* writer, MemAllocLinear* heap);

void JsonWriteStartObject(JsonWriter* writer);
void JsonWriteEndObject(JsonWriter* writer);

void JsonWriteStartArray(JsonWriter* writer);
void JsonWriteEndArray(JsonWriter* writer);

void JsonWriteKeyName(JsonWriter* writer, const char* keyName);

void JsonWriteValueString(JsonWriter* writer, const char* value);
void JsonWriteValueInteger(JsonWriter* writer, int64_t value);

void JsonWriteToFile(JsonWriter* writer, FILE* fp);

}

#endif