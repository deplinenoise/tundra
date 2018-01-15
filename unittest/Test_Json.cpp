#include "TestHarness.hpp"
#include "JsonParse.hpp"
#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"

using namespace t2;

class JsonTest : public ::testing::Test
{
protected:
  MemAllocHeap heap;
  MemAllocLinear alloc;
  MemAllocLinear scratch;
  char error_msg[1024];

protected:
  void SetUp() override
  {
    HeapInit(&heap, MB(4), HeapFlags::kDefault);
    LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
    LinearAllocInit(&scratch, &heap, MB(1), "json scratch");
  }

  void TearDown() override
  {
    LinearAllocDestroy(&scratch);
    LinearAllocDestroy(&alloc);
    HeapDestroy(&heap);
  }

};

TEST_F(JsonTest, Simple)
{
  char input[] = "  { \"foo\" : 8, \"bar\" : true }  ";
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_STREQ("", error_msg);
  ASSERT_NE(nullptr, v);

  const JsonObjectValue* obj = v->AsObject();
  ASSERT_NE(nullptr, obj);
  ASSERT_EQ(2, obj->m_Count);
  ASSERT_STREQ("foo", obj->m_Names[0]);
  ASSERT_STREQ("bar", obj->m_Names[1]);
  ASSERT_EQ(JsonValue::kNumber, obj->m_Values[0]->m_Type);
  ASSERT_EQ(8, int(obj->m_Values[0]->AsNumber()->m_Number));
  ASSERT_EQ(JsonValue::kBoolean, obj->m_Values[1]->m_Type);
  ASSERT_EQ(1, int(obj->m_Values[1]->AsBoolean()->m_Boolean));
}

TEST_F(JsonTest, EmptyObject)
{
  char input[] = "{}";
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_STREQ("", error_msg);
  ASSERT_NE(nullptr, v);

  ASSERT_EQ(JsonValue::kObject, v->m_Type);
  ASSERT_EQ(0, v->AsObject()->m_Count);
}

TEST_F(JsonTest, EmptyArray)
{
  char input[] = "[]";
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_STREQ("", error_msg);
  ASSERT_NE(nullptr, v);

  ASSERT_EQ(JsonValue::kArray, v->m_Type);
  ASSERT_EQ(0, v->AsArray()->m_Count);
}

TEST_F(JsonTest, ArraySingle)
{
  char input[] = "[\"foo\"]";
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_STREQ("", error_msg);
  ASSERT_NE(nullptr, v);

  ASSERT_EQ(JsonValue::kArray, v->m_Type);
  ASSERT_EQ(1, v->AsArray()->m_Count);
  ASSERT_EQ(JsonValue::kString, v->Elem(0)->m_Type);
  ASSERT_STREQ("foo", v->Elem(0)->AsString()->m_String);
}

TEST_F(JsonTest, ArrayMulti)
{
  char input[] = "[false, null, true]";
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_STREQ("", error_msg);
  ASSERT_NE(nullptr, v);

  const JsonArrayValue* array = v->AsArray();

  ASSERT_NE(nullptr, array);
  ASSERT_EQ(3, array->m_Count);
  ASSERT_EQ(JsonValue::kBoolean, array->m_Values[0]->m_Type);
  ASSERT_EQ(false, array->m_Values[0]->AsBoolean()->m_Boolean);
  ASSERT_EQ(JsonValue::kNull, array->m_Values[1]->m_Type);
  ASSERT_EQ(JsonValue::kBoolean, array->m_Values[2]->m_Type);
  ASSERT_EQ(true, array->m_Values[2]->AsBoolean()->m_Boolean);
}

TEST_F(JsonTest, StringEscapes)
{
  char input[] = "[\" foo \", \"\\\\\", \"\\n\\r\\t\\f\\b\\/\", \"\\u004C\\u004c\"]";
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_STREQ("", error_msg);
  ASSERT_NE(nullptr, v);

  const JsonArrayValue* array = v->AsArray();
  ASSERT_NE(nullptr, array);

  ASSERT_EQ(4, array->m_Count);
  ASSERT_EQ(JsonValue::kString, array->m_Values[0]->m_Type);
  ASSERT_EQ(JsonValue::kString, array->m_Values[1]->m_Type);
  ASSERT_EQ(JsonValue::kString, array->m_Values[2]->m_Type);
  ASSERT_EQ(JsonValue::kString, array->m_Values[3]->m_Type);
  ASSERT_STREQ(" foo ", array->m_Values[0]->AsString()->m_String);
  ASSERT_STREQ("\\", array->m_Values[1]->AsString()->m_String);
  ASSERT_STREQ("\n\r\t\f\b/", array->m_Values[2]->AsString()->m_String);
  ASSERT_STREQ("LL", array->m_Values[3]->AsString()->m_String);
}

TEST_F(JsonTest, Doubles)
{
  char input[] = "[123.456, -100.10, -1.0e10, 5e7]";
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_STREQ("", error_msg);
  ASSERT_NE(nullptr, v);

  const JsonArrayValue* array = v->AsArray();
  ASSERT_NE(nullptr, array);

  ASSERT_EQ(JsonValue::kArray, v->m_Type);
  ASSERT_EQ(4, array->m_Count);
  ASSERT_EQ(JsonValue::kNumber, array->m_Values[0]->m_Type);
  ASSERT_EQ(JsonValue::kNumber, array->m_Values[1]->m_Type);
  ASSERT_EQ(JsonValue::kNumber, array->m_Values[2]->m_Type);
  ASSERT_EQ(JsonValue::kNumber, array->m_Values[3]->m_Type);
  ASSERT_DOUBLE_EQ(array->m_Values[0]->AsNumber()->m_Number, 123.456);
  ASSERT_DOUBLE_EQ(array->m_Values[1]->AsNumber()->m_Number, -100.10);
  ASSERT_DOUBLE_EQ(array->m_Values[2]->AsNumber()->m_Number, -1.0e10);
  ASSERT_DOUBLE_EQ(array->m_Values[3]->AsNumber()->m_Number, 5e7);
}
