// tbasic2.cpp -- Regression test program, basic tests part 2
// $Id: tbasic2.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

static int ViewSize(c4_View v) {
  return v.GetSize();
}

void TestBasics2() {
  B(b20, Search sorted view, 0) {
    c4_IntProp p1("p1");
    c4_StringProp p2("p2");
    c4_View v1;
    v1.Add(p1[111] + p2["one"]);
    v1.Add(p1[222] + p2["two"]);
    v1.Add(p1[333] + p2["three"]);
    v1.Add(p1[345] + p2["four"]);
    v1.Add(p1[234] + p2["five"]);
    v1.Add(p1[123] + p2["six"]);
    c4_View v2 = v1.Sort();
    A(v2.GetSize() == 6);
    A(p1(v2[0]) == 111);
    A(p1(v2[1]) == 123);
    A(p1(v2[2]) == 222);
    A(p1(v2[3]) == 234);
    A(p1(v2[4]) == 333);
    A(p1(v2[5]) == 345);
    A(v2.Search(p1[123]) == 1);
    A(v2.Search(p1[100]) == 0);
    A(v2.Search(p1[200]) == 2);
    A(v2.Search(p1[400]) == 6);
    c4_View v3 = v1.SortOn(p2);
    A(v3.GetSize() == 6);
    A(p1(v3[0]) == 234);
    A(p1(v3[1]) == 345);
    A(p1(v3[2]) == 111);
    A(p1(v3[3]) == 123);
    A(p1(v3[4]) == 333);
    A(p1(v3[5]) == 222);
    A(v3.Search(p2["six"]) == 3);
    A(v3.Search(p2["aha"]) == 0);
    A(v3.Search(p2["gee"]) == 2);
    A(v3.Search(p2["wow"]) == 6);
    c4_View v4 = v1.SortOnReverse(p2, p2);
    A(v4.GetSize() == 6);
    A(p1(v4[0]) == 222);
    A(p1(v4[1]) == 333);
    A(p1(v4[2]) == 123);
    A(p1(v4[3]) == 111);
    A(p1(v4[4]) == 345);
    A(p1(v4[5]) == 234);
    A(v4.Search(p2["six"]) == 2);
    A(v4.Search(p2["aha"]) == 6);
    A(v4.Search(p2["gee"]) == 4);
    A(v4.Search(p2["wow"]) == 0);
  }
  E;

  B(b21, Memo property, 0) {
    c4_Row r1;
    c4_MemoProp p1("p1");
    c4_Bytes x1("hi!", 3);

    p1(r1) = x1;
    c4_Bytes x2 = p1(r1);
    A(x1 == x2);
  }
  E;

  B(b22, Stored view references, 0) {
    c4_ViewProp p1("p1");
    c4_View v1;

     {
      v1.Add(p1[c4_View()]);
    }

    // this works
    int n = ViewSize(p1.Get(v1[0]));
    A(n == 0);

    // this fails in 1.8b2 using MSVC 1.52 (tq_wvus)
    //
    // The compiler destructs temp c4_View once too often, or
    // what's more likely, fails to call the constructor once.
    //
    // So for MSVC 1.52: use prop.Get(rowref) for subviews,
    // or immediately assign the result to a c4_View object,
    // do not pass a "prop (rowref)" expression as argument.

#if _MSC_VER != 800     
    int m = ViewSize(p1(v1[0]));
    A(m == 0);
#endif 
  }
  E;

  B(b23, Sort comparison fix, 0) { // 1.9e: compare buffering problem
    c4_DoubleProp p1("p1");
    c4_View v1;
    for (int i = 0; i < 100; ++i)
      v1.Add(p1[99-i]);
    c4_View v2 = v1.Sort();
    A(v2.GetSize() == 100);
    for (int j = 0; j < 100; ++j) {
      A(p1(v1[j]) == (double)99-j);
      A(p1(v2[j]) == (double)j);
    }
  }
  E;

  B(b24, Custom view comparisons, 0) { // 1.9f: more compare cache problems
    c4_IntProp p1("p1");
    c4_FloatProp p2("p2");
    c4_DoubleProp p3("p3");
    c4_IntProp p4("p4");
    c4_View v1;
    v1.Add(p1[2] + p2[2] + p3[2]);
    v1.Add(p1[1] + p2[1] + p3[1]);
    v1.Add(p1[3] + p2[3] + p3[3]);
    A(v1.GetSize() == 3);
    A((int)p1(v1[0]) > (int)p1(v1[1]));
    A((float)p2(v1[0]) > (float)p2(v1[1]));
    A((double)p3(v1[0]) > (double)p3(v1[1]));
    A((int)p1(v1[0]) < (int)p1(v1[2]));
    A((float)p2(v1[0]) < (float)p2(v1[2]));
    A((double)p3(v1[0]) < (double)p3(v1[2]));
    c4_View v2 = v1.Unique();
    A(v2.GetSize() == 3);
    A((int)p1(v2[0]) != (int)p1(v2[1]));
    A((float)p2(v2[0]) != (float)p2(v2[1]));
    A((double)p3(v2[0]) != (double)p3(v2[1]));
    A((int)p1(v2[0]) != (int)p1(v2[2]));
    A((float)p2(v2[0]) != (float)p2(v2[2]));
    A((double)p3(v2[0]) != (double)p3(v2[2]));
    v1.Add(p1[2] + p2[2] + p3[2]);
    v1.Add(p1[1] + p2[1] + p3[1]);
    v1.Add(p1[3] + p2[3] + p3[3]);
    c4_View v3 = v1.Unique();
    A(v3.GetSize() == 3);
    A((int)p1(v3[0]) != (int)p1(v3[1]));
    A((float)p2(v3[0]) != (float)p2(v3[1]));
    A((double)p3(v3[0]) != (double)p3(v3[1]));
    A((int)p1(v3[0]) != (int)p1(v3[2]));
    A((float)p2(v3[0]) != (float)p2(v3[2]));
    A((double)p3(v3[0]) != (double)p3(v3[2]));
    c4_View v4 = v1.Counts(p1, p4);
    A(v4.GetSize() == 3);
    c4_View v5 = v1.Counts(p2, p4);
    A(v5.GetSize() == 3); // this failed in 1.9f
    c4_View v6 = v1.Counts(p3, p4);
    A(v6.GetSize() == 3); // this failed in 1.9f
  }
  E;

  B(b25, Copy row from derived, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    c4_View v2 = v1.Select(p1[222]);
    A(v2.GetSize() == 1);
    A(p1(v2[0]) == 222);
    c4_Row r = v2[0];
    A(p1(r) == 222); // 1.9g: failed because SetAt did not remap
  }
  E;

  B(b26, Partial memo field access, 0) {
    c4_BytesProp p1("p1");
    c4_View v1;
    v1.Add(p1[c4_Bytes("12345", 5)]);
    A(v1.GetSize() == 1);
    c4_Bytes buf = p1(v1[0]);
    A(buf.Size() == 5);
    A(buf == c4_Bytes("12345", 5));
    buf = p1(v1[0]).Access(1, 3);
    A(buf == c4_Bytes("234", 3));
    p1(v1[0]).Modify(c4_Bytes("ab", 2), 2, 0);
    buf = p1(v1[0]);
    A(buf == c4_Bytes("12ab5", 5));
    p1(v1[0]).Modify(c4_Bytes("ABC", 3), 1, 2);
    buf = p1(v1[0]);
    A(buf == c4_Bytes("1ABCab5", 7));
    p1(v1[0]).Modify(c4_Bytes("xyz", 3), 2,  - 2);
    buf = p1(v1[0]);
    A(buf == c4_Bytes("1Axyz", 5));
    p1(v1[0]).Modify(c4_Bytes("3456", 4), 4, 0);
    buf = p1(v1[0]);
    A(buf == c4_Bytes("1Axy3456", 8));
  }
  E;

  B(b27, Copy value to another row, 0) {
    c4_StringProp p1("p1");
    c4_View v1;
    v1.SetSize(2);
    p1(v1[1]) = "abc";
    // next assert fails in MacOS X 10.2.1 "Jaguar" with 2.4.7
    // seems bug in gcc 3.1, -O i.s.o. -O2 works [jcw 21oct02]
    A((const char*)(p1(v1[0])) == (c4_String)"");
    A((const char*)(p1(v1[1])) == (c4_String)"abc");

    // fails in 2.4.0, reported by Jerry McRae, August 2001
    p1(v1[0]) = (const char*)p1(v1[1]);
    // MacOS 10.2.3 gcc 3.1 dec 2002 is still weird, inserting the
    // following code (which should be a noop) *fixes* the assert!
    //const char* q = p1 (v1[1]);
    A((c4_String)(const char*)(p1(v1[0])) == (c4_String)"abc");
  }
  E;
}
