#include "PathUtil.hpp"
#include "TestHarness.hpp"

using namespace t2;

TEST(PathUtil, Init)
{
  static const struct
  {
    PathType::Enum  type;
    const char     *input;
    const char     *expected_output;
    bool            expect_absolute;
    int             expect_segs;
    int             expect_seg_lens[8];
  }
  test_data[] =
  {
    { PathType::kUnix, "",                "",                     false,  0, {} },
    { PathType::kUnix, "/",               "/",                    true,   0, {} },
    { PathType::kUnix, "////",            "/",                    true,   0, {} },
    { PathType::kUnix, "foo.c",           "foo.c",                false,  1, { 5 } },
    { PathType::kUnix, "/foo.c",          "/foo.c",               true,   1, { 5 } },
    { PathType::kUnix, "foo/bar.c",       "foo/bar.c",            false,  2, { 3, 5 } },
    { PathType::kUnix, "/foo/bar.c",      "/foo/bar.c",           true,   2, { 3, 5 } },
    { PathType::kUnix, "/foo///bar.c",    "/foo/bar.c",           true,   2, { 3, 5 } },
    { PathType::kUnix, "/foo///bar.c/",   "/foo/bar.c",           true,   2, { 3, 5 } },
    { PathType::kUnix, "/foo/./bar.c/",   "/foo/bar.c",           true,   2, { 3, 5 } },
    { PathType::kUnix, "/foo/../bar.c/",  "/bar.c",               true,   1, { 5 } },
    { PathType::kUnix, "foo/../../bar.c", "../bar.c",             false,  1, { 5 } },
    { PathType::kUnix, "../../../bar.c",  "../../../bar.c",       false,  1, { 5 } },
    { PathType::kUnix, "./././bar.c",     "bar.c",                false,  1, { 5 } },
    { PathType::kUnix, ".",               "",                     false,  0, {}},
    { PathType::kUnix, "././.",           "",                     false,  0, {}},

    { PathType::kWindows, "",                "",                     false,  0, {} },
    { PathType::kWindows, "\\",              "\\",                   true,   0, {} },
    { PathType::kWindows, "////",            "\\",                   true,   0, {} },
    { PathType::kWindows, "foo.c",           "foo.c",                false,  1, { 5 } },
    { PathType::kWindows, "/foo.c",          "\\foo.c",              true,   1, { 5 } },
    { PathType::kWindows, "foo/bar.c",       "foo\\bar.c",           false,  2, { 3, 5 } },
    { PathType::kWindows, "/foo/bar.c",      "\\foo\\bar.c",         true,   2, { 3, 5 } },
    { PathType::kWindows, "/foo///bar.c",    "\\foo\\bar.c",         true,   2, { 3, 5 } },
    { PathType::kWindows, "/foo///bar.c/",   "\\foo\\bar.c",         true,   2, { 3, 5 } },
    { PathType::kWindows, "/foo/./bar.c/",   "\\foo\\bar.c",         true,   2, { 3, 5 } },
    { PathType::kWindows, "/foo/../bar.c/",  "\\bar.c",              true,   1, { 5 } },
    { PathType::kWindows, "foo/../../bar.c", "..\\bar.c",            false,  1, { 5 } },
    { PathType::kWindows, "../../../bar.c",  "..\\..\\..\\bar.c",    false,  1, { 5 } },
    { PathType::kWindows, "./././bar.c",     "bar.c",                false,  1, { 5 } },
    { PathType::kWindows, ".",               "",                     false,  0, { } },
    { PathType::kWindows, "././.",           "",                     false,  0, { } },
    { PathType::kWindows, "x:\\foo",         "x:\\foo",              true,   2, { 2, 3 } },
    { PathType::kWindows, "x:/foo",          "x:\\foo",              true,   2, { 2, 3 } },
  };

  for (size_t i = 0; i < ARRAY_SIZE(test_data); ++i)
  {
    PathBuffer path;
    PathInit(&path, test_data[i].input, test_data[i].type);
    ASSERT_EQ(test_data[i].expect_absolute, PathIsAbsolute(&path));
    ASSERT_EQ(test_data[i].expect_segs, path.m_SegCount);

    for (int s = 0; s < path.m_SegCount; ++s)
    {
      ASSERT_EQ(test_data[i].expect_seg_lens[s], path.SegLength(s));
    }

    char buffer[kMaxPathLength];
    PathFormat(buffer, &path);
    ASSERT_STREQ(test_data[i].expected_output, buffer);
  }
}

TEST(PathUtil, Concat)
{
  static const struct
  {
    PathType::Enum  type;
    const char     *a, *b;
    const char     *expected_output;
  }
  test_data[] =
  {
    { PathType::kUnix,    "",                "",                     "" },
    { PathType::kUnix,    "",                "foo.c",                "foo.c" },
    { PathType::kUnix,    "/",               "foo.c",                "/foo.c" },
    { PathType::kUnix,    "/bar",            "foo.c",                "/bar/foo.c" },
    { PathType::kUnix,    "/bar",            "../foo.c",             "/foo.c" },
    { PathType::kUnix,    "/bar",            "../../foo.c",          "/foo.c" },
    { PathType::kUnix,    "/bar",            "/foo.c",               "/foo.c" },
    { PathType::kUnix,    "/a/b/c/d",        "e/f/g.h",              "/a/b/c/d/e/f/g.h" },
    { PathType::kUnix,    "a/b/c/d",         "../..",                "a/b" },
    { PathType::kWindows, "\\foo",           "bar.h",                "\\foo\\bar.h" },
    { PathType::kWindows, "x:\\foo",         "bar.h",                "x:\\foo\\bar.h" },
    { PathType::kWindows, "x:\\foo",         "..\\bar.h",            "x:\\bar.h" },
    { PathType::kWindows, "x:\\foo",         "..\\..\\bar.h",        "x:\\bar.h" },
  };

  for (size_t i = 0; i < ARRAY_SIZE(test_data); ++i)
  {
    PathBuffer a, b;
    PathInit(&a, test_data[i].a, test_data[i].type);
    PathInit(&b, test_data[i].b, test_data[i].type);

    PathConcat(&a, &b);

    char buffer[kMaxPathLength];
    PathFormat(buffer, &a);
    ASSERT_STREQ(test_data[i].expected_output, buffer);
  }
}
