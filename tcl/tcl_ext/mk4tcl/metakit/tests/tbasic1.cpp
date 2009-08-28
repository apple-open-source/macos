// tbasic1.cpp -- Regression test program, basic tests part 1
// $Id: tbasic1.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestBasics1() {
  B(b00, Should fail, 0) {
    A(false);
  }
  E;

  B(b01, Should succeed, 0) {
    A(sizeof(t4_byte) == 1);
    A(sizeof(short) == 2);
    A(sizeof(t4_i32) == 4);
    A(sizeof(float) == 4);
    A(sizeof(double) == 8);
  }
  E;

  B(b02, Int property, 0) {
    c4_Row r1;
    c4_IntProp p1("p1");
    p1(r1) = 1234567890L;
    long x1 = p1(r1);
    A(x1 == 1234567890L);
  }
  E;

#if !q4_TINY
  B(b03, Float property, 0) {
    c4_Row r1;
    c4_FloatProp p1("p1");
    p1(r1) = 123.456;
    double x1 = p1(r1);
    A((float)x1 == (float)123.456);
  }
  E;
#endif 

  B(b04, String property, 0) {
    c4_Row r1;
    c4_StringProp p1("p1");
    p1(r1) = "abc";
    const char *x1 = p1(r1);
    A((c4_String)x1 == "abc");
  }
  E;

  B(b05, View property, 0) {
    c4_View v1;
    c4_Row r1;
    c4_ViewProp p1("p1");
    p1(r1) = v1;
    c4_View x1 = p1(r1);
    // compare cursors to make sure this is the same sequence
    A(x1[0] == v1[0]);
  }
  E;

  B(b06, View construction, 0) {
    c4_IntProp p1("p1"), p2("p2"), p3("p3");
    c4_View v1 = (p1, p3, p2);
    A(v1.FindProperty(p1.GetId()) == 0);
    A(v1.FindProperty(p2.GetId()) == 2);
    A(v1.FindProperty(p3.GetId()) == 1);
  }
  E;

  B(b07, Row manipulation, 0) {
    c4_StringProp p1("p1"), p2("p2");
    c4_IntProp p3("p3");
    c4_Row r1;
    p1(r1) = "look at this";
    const char *x1 = p1(r1);
    A(x1 == (c4_String)"look at this");
    r1 = p1["what's in a"] + p2["name..."];
    c4_String t = (const char*)p2(r1);
    p1(r1) = t + (const char*)(p1(r1));
    p2(r1) = p1(r1);
    c4_String x2 = (const char*)p1(r1); // 2000-03-16, store as c4_String
    A(x2 == "name...what's in a");
    // the above change avoids an evaluation order issue in assert below
    A(x2 == p2(r1));
    p3(r1) = 12345;
    p3(r1) = p3(r1) + 123;
    int x3 = p3(r1);
    A(x3 == 12345+123);
  }
  E;

  B(b08, Row expressions, 0) {
    c4_StringProp p1("p1"), p2("p2");
    c4_IntProp p3("p3");
    c4_Row r1;
    c4_View v1 = (p1, p2, p3);
    v1.SetSize(5);
    r1 = v1[1];
    v1[2] = v1[1];
    v1[3] = r1;
    v1[4] = v1[4];
    r1 = r1;
  }
  E;

  B(b09, View manipulation, 0) {
    c4_StringProp p1("p1"), p2("p2");
    c4_Row r1 = p1["One"] + p2["Two"];
    c4_Row r2;
    c4_View v1;
    v1.Add(r1);
    v1.Add(r2);
    v1.Add(r1);
    A(v1.GetSize() == 3);
    A(v1[0] == r1);
    A(v1[1] == r2);
    A(v1[2] == r1);
    v1.RemoveAt(1, 1);
    A(v1.GetSize() == 2);
    A(v1[0] == r1);
    A(v1[0] == v1[1]);
  }
  E;

  B(b10, View sorting, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    c4_View v2 = v1.Sort();
    A(v2.GetSize() == 6);
    A(p1(v2[0]) == 111);
    A(p1(v2[1]) == 123);
    A(p1(v2[2]) == 222);
    A(p1(v2[3]) == 234);
    A(p1(v2[4]) == 333);
    A(p1(v2[5]) == 345);
  }
  E;

  B(b11, View selection, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    c4_View v2 = v1.SelectRange(p1[200], p1[333]);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
  }
  E;

  B(b12, Add after remove, 0) {
    c4_StringProp p1("p1");
    c4_View v1;
    v1.Add(p1["abc"]);
    A(v1.GetSize() == 1);
    v1.RemoveAt(0);
    A(v1.GetSize() == 0);
    v1.Add(p1["def"]);
    A(v1.GetSize() == 1);
  }
  E;

  B(b13, Clear view entry, 0) {
    c4_IntProp p1("p1");
    c4_View v1;

    v1.Add(p1[123]);
    A(v1.GetSize() == 1);
    A(p1(v1[0]) == 123);

    v1[0] = c4_Row();
    A(v1.GetSize() == 1);
    A(p1(v1[0]) == 0);
  }
  E;

  B(b14, Empty view outlives temp storage, 0) {
    c4_View v1;
    c4_Storage s1;
    v1 = s1.GetAs("a[p1:I,p2:S]");
  }
  E;

  B(b15, View outlives temp storage, 0) {
    c4_IntProp p1("p1");
    c4_View v1;

     {
      c4_Storage s1;
      v1 = s1.GetAs("a[p1:I,p2:S]");
      v1.Add(p1[123]);
    }

    // 19990916 - semantics changed, view now 1 row, but 0 props
    A(v1.GetSize() == 1);
    A(v1.NumProperties() == 0);
    //A(p1 (v1[0]) == 123);
  }
  E;

  B(b16, View outlives cleared temp storage, 0) {
    c4_IntProp p1("p1");
    c4_View v1;

     {
      c4_Storage s1;
      v1 = s1.GetAs("a[p1:I,p2:S]");
      v1.Add(p1[123]);
      v1.RemoveAll();
    }

    A(v1.GetSize() == 0);
    v1.Add(p1[123]);
    A(v1.GetSize() == 1);
    A(p1(v1[0]) == 123);
  }
  E;

#if !q4_TINY
  B(b17, Double property, 0) {
    c4_Row r1;
    c4_DoubleProp p1("p1");
    p1(r1) = 1234.5678;
    double x1 = p1(r1);
    A(x1 == (double)1234.5678);
  }
  E;
#endif 

  B(b18, SetAtGrow usage, 0) {
    c4_IntProp p1("p1");
    c4_View v1;

    v1.SetAtGrow(3, p1[333]);
    v1.SetAtGrow(1, p1[111]);
    v1.SetAtGrow(5, p1[555]);

    A(v1.GetSize() == 6);
    A(p1(v1[1]) == 111);
    A(p1(v1[3]) == 333);
    A(p1(v1[5]) == 555);
  }
  E;

  B(b19, Bytes property, 0) {
    c4_Row r1;
    c4_BytesProp p1("p1");
    c4_Bytes x1("hi!", 3);

    p1(r1) = x1;
    c4_Bytes x2 = p1(r1);
    A(x1 == x2);
  }
  E;
}
