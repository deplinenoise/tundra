#include "TestHarness.hpp"
#include "JsonParse.hpp"
#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"

using namespace t2;

BEGIN_TEST_CASE("JsonSimple")
{
  char input[] = "  { \"foo\" : 8, \"bar\" : true }  ";
  MemAllocHeap heap;
  HeapInit(&heap, MB(4), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;
  LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(1), "json scratch");

  char error_msg[1024];
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_EQUAL_STRING(error_msg, "");
  ASSERT_NOT_EQUAL(v, nullptr);

  const JsonObjectValue* obj = v->AsObject();
  ASSERT_NOT_EQUAL(obj, nullptr);
  ASSERT_EQUAL(obj->m_Count, 2);
  ASSERT_EQUAL_STRING(obj->m_Names[0], "foo");
  ASSERT_EQUAL_STRING(obj->m_Names[1], "bar");
  ASSERT_EQUAL(obj->m_Values[0]->m_Type, JsonValue::kNumber);
  ASSERT_EQUAL(int(obj->m_Values[0]->AsNumber()->m_Number), 8);
  ASSERT_EQUAL(obj->m_Values[1]->m_Type, JsonValue::kBoolean);
  ASSERT_EQUAL(int(obj->m_Values[1]->AsBoolean()->m_Boolean), 1);

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("JsonEmptyObject")
{
  char input[] = "{}";
  MemAllocHeap heap;
  HeapInit(&heap, MB(4), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;
  LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(1), "json scratch");

  char error_msg[1024];
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_EQUAL_STRING(error_msg, "");
  ASSERT_NOT_EQUAL(v, nullptr);

  ASSERT_EQUAL(v->m_Type, JsonValue::kObject);
  ASSERT_EQUAL(v->AsObject()->m_Count, 0);

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("JsonEmptyArray")
{
  char input[] = "[]";
  MemAllocHeap heap;
  HeapInit(&heap, MB(4), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;
  LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(1), "json scratch");

  char error_msg[1024];
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_EQUAL_STRING(error_msg, "");
  ASSERT_NOT_EQUAL(v, nullptr);

  ASSERT_EQUAL(v->m_Type, JsonValue::kArray);
  ASSERT_EQUAL(v->AsArray()->m_Count, 0);

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("JsonArraySingle")
{
  char input[] = "[\"foo\"]";
  MemAllocHeap heap;
  HeapInit(&heap, MB(4), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;
  LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(1), "json scratch");

  char error_msg[1024];
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_EQUAL_STRING(error_msg, "");
  ASSERT_NOT_EQUAL(v, nullptr);

  ASSERT_EQUAL(v->m_Type, JsonValue::kArray);
  ASSERT_EQUAL(v->AsArray()->m_Count, 1);
  ASSERT_EQUAL(v->Elem(0)->m_Type, JsonValue::kString);
  ASSERT_EQUAL_STRING(v->Elem(0)->AsString()->m_String, "foo");

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("JsonArrayMulti")
{
  char input[] = "[false, null, true]";
  MemAllocHeap heap;
  HeapInit(&heap, MB(4), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;
  LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(1), "json scratch");

  char error_msg[1024];
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_EQUAL_STRING(error_msg, "");
  ASSERT_NOT_EQUAL(v, nullptr);

  const JsonArrayValue* array = v->AsArray();

  ASSERT_NOT_EQUAL(array, nullptr);
  ASSERT_EQUAL(array->m_Count, 3);
  ASSERT_EQUAL(array->m_Values[0]->m_Type, JsonValue::kBoolean);
  ASSERT_EQUAL(array->m_Values[0]->AsBoolean()->m_Boolean, false);
  ASSERT_EQUAL(array->m_Values[1]->m_Type, JsonValue::kNull);
  ASSERT_EQUAL(array->m_Values[2]->m_Type, JsonValue::kBoolean);
  ASSERT_EQUAL(array->m_Values[2]->AsBoolean()->m_Boolean, true);

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("JsonStringEscapes")
{
  char input[] = "[\" foo \", \"\\\\\", \"\\n\\r\\t\\f\\b\\/\", \"\\u004C\\u004c\"]";
  MemAllocHeap heap;
  HeapInit(&heap, MB(4), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;
  LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(1), "json scratch");

  char error_msg[1024];
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_EQUAL_STRING(error_msg, "");
  ASSERT_NOT_EQUAL(v, nullptr);

  const JsonArrayValue* array = v->AsArray();
  ASSERT_NOT_EQUAL(array, nullptr);

  ASSERT_EQUAL(array->m_Count, 4);
  ASSERT_EQUAL(array->m_Values[0]->m_Type, JsonValue::kString);
  ASSERT_EQUAL(array->m_Values[1]->m_Type, JsonValue::kString);
  ASSERT_EQUAL(array->m_Values[2]->m_Type, JsonValue::kString);
  ASSERT_EQUAL(array->m_Values[3]->m_Type, JsonValue::kString);
  ASSERT_EQUAL_STRING(array->m_Values[0]->AsString()->m_String, " foo ");
  ASSERT_EQUAL_STRING(array->m_Values[1]->AsString()->m_String, "\\");
  ASSERT_EQUAL_STRING(array->m_Values[2]->AsString()->m_String, "\n\r\t\f\b/");
  ASSERT_EQUAL_STRING(array->m_Values[3]->AsString()->m_String, "LL");

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("JsonDoubles")
{
  char input[] = "[123.456, -100.10, -1.0e10, 5e7]";
  MemAllocHeap heap;
  HeapInit(&heap, MB(4), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;
  LinearAllocInit(&alloc, &heap, MB(1), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(1), "json scratch");

  char error_msg[1024];
  const JsonValue* v = JsonParse(input, &alloc, &scratch, error_msg);

  ASSERT_EQUAL_STRING(error_msg, "");
  ASSERT_NOT_EQUAL(v, nullptr);

  const JsonArrayValue* array = v->AsArray();
  ASSERT_NOT_EQUAL(array, nullptr);

  ASSERT_EQUAL(v->m_Type, JsonValue::kArray);
  ASSERT_EQUAL(array->m_Count, 4);
  ASSERT_EQUAL(array->m_Values[0]->m_Type, JsonValue::kNumber);
  ASSERT_EQUAL(array->m_Values[1]->m_Type, JsonValue::kNumber);
  ASSERT_EQUAL(array->m_Values[2]->m_Type, JsonValue::kNumber);
  ASSERT_EQUAL(array->m_Values[3]->m_Type, JsonValue::kNumber);
  ASSERT_EQUAL_FLOAT(array->m_Values[0]->AsNumber()->m_Number, 123.456, 0.0001);
  ASSERT_EQUAL_FLOAT(array->m_Values[1]->AsNumber()->m_Number, -100.10, 0.0001);
  ASSERT_EQUAL_FLOAT(array->m_Values[2]->AsNumber()->m_Number, -1.0e10, 0.001);
  ASSERT_EQUAL_FLOAT(array->m_Values[3]->AsNumber()->m_Number, 5e7, 0.001);

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE
