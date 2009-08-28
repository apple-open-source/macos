// textend.cpp -- Regression test program, commit extend tests
// $Id: textend.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

const int kSize1 = 41;
const int kSize2 = 85;

void TestExtend() {
  B(e01, Extend new file, 0)W(e01a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("e01a", 2);
    A(s1.Strategy().FileSize() == 0);
    c4_View v1 = s1.GetAs("a[p1:I]");
    v1.Add(p1[123]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize1);
    v1.Add(p1[456]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize2);
  }
  D(e01a);
  R(e01a);
  E;

  B(e02, Extend committing twice, 0)W(e02a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("e02a", 2);
    A(s1.Strategy().FileSize() == 0);
    c4_View v1 = s1.GetAs("a[p1:I]");
    v1.Add(p1[123]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize1);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize1);
    v1.Add(p1[456]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize2);
  }
  D(e02a);
  R(e02a);
  E;

  B(e03, Read during extend, 0)W(e03a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("e03a", 2);
    A(s1.Strategy().FileSize() == 0);
    c4_View v1 = s1.GetAs("a[p1:I]");
    v1.Add(p1[123]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize1);

     {
      c4_Storage s2("e03a", 0);
      c4_View v2 = s2.View("a");
      A(v2.GetSize() == 1);
      A(p1(v2[0]) == 123);
    }

    v1.Add(p1[456]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize2);

     {
      c4_Storage s3("e03a", 0);
      c4_View v3 = s3.View("a");
      A(v3.GetSize() == 2);
      A(p1(v3[0]) == 123);
      A(p1(v3[1]) == 456);
    }
  }
  D(e03a);
  R(e03a);
  E;

  B(e04, Extend during read, 0)W(e04a);
   {
    c4_IntProp p1("p1");

     {
      c4_Storage s1("e04a", 2);
      A(s1.Strategy().FileSize() == 0);
      c4_View v1 = s1.GetAs("a[p1:I]");
      v1.Add(p1[123]);
      s1.Commit();
      A(s1.Strategy().FileSize() == kSize1);
    }

    c4_Storage s2("e04a", 0);
    c4_View v2 = s2.View("a");
    A(v2.GetSize() == 1);
    A(p1(v2[0]) == 123);

    c4_Storage s3("e04a", 0); { // open, don't load

    
      c4_Storage s4("e04a", 2);
      A(s4.Strategy().FileSize() == kSize1);
      c4_View v4 = s4.View("a");
      v4.Add(p1[123]);
      s4.Commit();
      A(s4.Strategy().FileSize() > kSize1); // == kSize2);
    }

    c4_View v2a = s2.View("a");
    A(v2a.GetSize() == 1);
    A(p1(v2a[0]) == 123);

    c4_View v3 = s3.View("a");
    A(v3.GetSize() == 1);
    A(p1(v3[0]) == 123);

  }
  D(e04a);
  R(e04a);
  E;

  B(e05, Test memory mapping, 0)W(e05a);
   {
    // this is not a test of MK, but of the underlying system code

     {
      c4_FileStrategy fs;
      bool f1 = fs.DataOpen("e05a", 1);
      A(!f1);
      fs.DataWrite(0, "hi!", 3);
      A(fs._failure == 0);
      A(fs.FileSize() == 3);
      fs.DataCommit(0);
      A(fs.FileSize() == 3);
      fs.ResetFileMapping();
      if (fs._mapStart != 0) {
        A(fs._dataSize == 3);
        c4_String s((char*)fs._mapStart, 3);
        A(s == "hi!");
      }
      fs.DataWrite(3, "hello", 5);
      A(fs._failure == 0);
      A(fs.FileSize() == 8);
      fs.DataCommit(0);
      A(fs.FileSize() == 8);
      if (fs._mapStart != 0) {
        A(fs._dataSize == 3);
        c4_String s((char*)fs._mapStart, 8);
        A(s == "hi!hello");
      }
      fs.DataWrite(100, "wow!", 4);
      A(fs._failure == 0);
      A(fs.FileSize() == 104);
      fs.DataCommit(0);
      A(fs.FileSize() == 104);
      fs.ResetFileMapping();
      if (fs._mapStart != 0) {
        A(fs._dataSize == 104);
        c4_String s((char*)fs._mapStart + 100, 4);
        A(s == "wow!");
      }
    }

    // clear the file, so dump doesn't choke on it
    FILE *fp = fopen("e05a", "w");
    A(fp != 0);
    fclose(fp);

  }
  D(e05a);
  R(e05a);
  E;

  B(e06, Rollback during extend, 0)W(e06a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("e06a", 2);
    A(s1.Strategy().FileSize() == 0);
    c4_View v1 = s1.GetAs("a[p1:I]");
    v1.Add(p1[123]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize1);

    c4_Storage s2("e06a", 0);
    c4_View v2 = s2.View("a");
    A(v2.GetSize() == 1);
    A(p1(v2[0]) == 123);

    v1.Add(p1[456]);
    s1.Commit();
    A(s1.Strategy().FileSize() == kSize2);
#if 0
    /* fails on NT + Samba, though it works fine with mmap'ing disabled */
    s2.Rollback();

    c4_View v2a = s2.View("a");
    A(v2a.GetSize() == 2);
    A(p1(v2a[0]) == 123);
    A(p1(v2a[1]) == 456);
#else 
    c4_View v2a = s2.View("a");
    A(v2a.GetSize() == 1);
    A(p1(v2a[0]) == 123);
#endif 
  }
  D(e06a);
  R(e06a);
  E;
}
