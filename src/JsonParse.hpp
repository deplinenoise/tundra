#ifndef JSONPARSE_HPP
#define JSONPARSE_HPP

#include "Common.hpp"
#include <string.h>

namespace t2
{

struct MemAllocLinear;

struct JsonValue
{
  enum Type
  {
    kNull,
    kBoolean,
    kObject,
    kArray,
    kString,
    kNumber
  };

  Type      m_Type;

  const struct JsonObjectValue* AsObject() const;
  const struct JsonNumberValue* AsNumber() const;
  const struct JsonStringValue* AsString() const;
  const struct JsonArrayValue* AsArray() const;
  const struct JsonBooleanValue* AsBoolean() const;

  double GetNumber() const;
  const char* GetString() const;
  bool GetBoolean() const;

  const JsonValue* Elem(size_t index) const;
  const JsonValue* Find(const char* key) const;
};

struct JsonBooleanValue : JsonValue
{
  bool m_Boolean;
};

struct JsonNumberValue : JsonValue
{
  double m_Number;
};

struct JsonStringValue : JsonValue
{
  const char* m_String;
};

struct JsonArrayValue : JsonValue
{
  size_t        m_Count;
  const JsonValue**   m_Values;
};

struct JsonObjectValue : JsonValue
{
  size_t        m_Count;
  const char**  m_Names;
  const JsonValue**   m_Values;
};

inline const JsonObjectValue* JsonValue::AsObject() const
{
  if (kObject == m_Type)
    return static_cast<const JsonObjectValue*>(this);

  return nullptr;
}

inline const JsonArrayValue* JsonValue::AsArray() const
{
  if (kArray == m_Type)
    return static_cast<const JsonArrayValue*>(this);

  return nullptr;
}

inline const JsonBooleanValue* JsonValue::AsBoolean() const
{
  if (kBoolean == m_Type)
    return static_cast<const JsonBooleanValue*>(this);

  return nullptr;
}

inline const JsonStringValue* JsonValue::AsString() const
{
  if (kString == m_Type)
    return static_cast<const JsonStringValue*>(this);

  return nullptr;
}

inline const JsonNumberValue* JsonValue::AsNumber() const
{
  if (kNumber == m_Type)
    return static_cast<const JsonNumberValue*>(this);

  return nullptr;
}

inline const JsonValue* JsonValue::Elem(size_t index) const
{
  const JsonArrayValue* array = AsArray();
  CHECK(index < array->m_Count);
  return array->m_Values[index];
}

inline const JsonValue* JsonValue::Find(const char* key) const
{
  const JsonObjectValue* obj = AsObject();
  for (size_t i = 0, count = obj->m_Count; i < count; ++i)
  {
    if (0 == strcmp(obj->m_Names[i], key))
      return obj->m_Values[i];
  }

  return nullptr;
}

inline double JsonValue::GetNumber() const
{
  const JsonNumberValue* num = AsNumber();
  CHECK(num);
  return num->m_Number;
}

inline const char* JsonValue::GetString() const
{
  const JsonStringValue* str = AsString();
  CHECK(str);
  return str->m_String;
}

inline bool JsonValue::GetBoolean() const
{
  const JsonBooleanValue* b = AsBoolean();
  CHECK(b);
  return b->m_Boolean;
}

const JsonValue* JsonParse(
    char *buffer,
    MemAllocLinear* allocator,
    MemAllocLinear* scratch,
    char (&error_message)[1024]);

}


#endif
