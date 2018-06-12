#include "JsonWriter.hpp"
#include <stdio.h>

namespace t2
{

void JsonWriteInit(JsonWriter* writer, MemAllocHeap* heap)
{
    writer->m_Heap = heap;
    BufferInitWithCapacity(&writer->m_Buffer, heap, 200);
    JsonWriteReset(writer);
}

void JsonWriteDestroy(JsonWriter* writer)
{
    BufferDestroy(&writer->m_Buffer, writer->m_Heap);
}

void JsonWriteReset(JsonWriter* writer)
{
    BufferClear(&writer->m_Buffer);
    writer->m_PrependComma = false;
}

void JsonWriteStartObject(JsonWriter* writer)
{
    if (writer->m_PrependComma)
        BufferAppendOne(&writer->m_Buffer, writer->m_Heap, ',');

    BufferAppendOne(&writer->m_Buffer, writer->m_Heap, '{');
    writer->m_PrependComma = false;
}

void JsonWriteEndObject(JsonWriter* writer)
{
    BufferAppendOne(&writer->m_Buffer, writer->m_Heap, '}');
    writer->m_PrependComma = true;
}

void JsonWriteStartArray(JsonWriter* writer)
{
    if (writer->m_PrependComma)
        BufferAppendOne(&writer->m_Buffer, writer->m_Heap, ',');

    BufferAppendOne(&writer->m_Buffer, writer->m_Heap, '[');
    writer->m_PrependComma = false;
}

void JsonWriteEndArray(JsonWriter* writer)
{
    BufferAppendOne(&writer->m_Buffer, writer->m_Heap, ']');
    writer->m_PrependComma = true;
}

void JsonWriteKeyName(JsonWriter* writer, const char* keyName)
{
    JsonWriteValueString(writer, keyName);
    BufferAppendOne(&writer->m_Buffer, writer->m_Heap, ':');
    writer->m_PrependComma = false;
}

void JsonWriteValueString(JsonWriter* writer, const char* value)
{
    if (writer->m_PrependComma)
        BufferAppendOne(&writer->m_Buffer, writer->m_Heap, ',');

    BufferAppendOne(&writer->m_Buffer, writer->m_Heap, '"');

    while (*value != 0)
    {
        char ch = *(value++);
        if (ch == '"')
        {
            BufferAppend(&writer->m_Buffer, writer->m_Heap, "\\\"", 2);
        }
        else if (ch == '\\')
        {
            BufferAppend(&writer->m_Buffer, writer->m_Heap, "\\\\", 2);
        }
        else
        {
            BufferAppendOne(&writer->m_Buffer, writer->m_Heap, ch);
        }
    }

    BufferAppendOne(&writer->m_Buffer, writer->m_Heap, '"');

    writer->m_PrependComma = true;
}

void JsonWriteValueInteger(JsonWriter* writer, int64_t value)
{
    if (writer->m_PrependComma)
        BufferAppendOne(&writer->m_Buffer, writer->m_Heap, ',');

    char buf[50];
    snprintf(buf, 50, "%lli", value);

    BufferAppend(&writer->m_Buffer, writer->m_Heap, buf, strlen(buf));

    writer->m_PrependComma = true;
}

}