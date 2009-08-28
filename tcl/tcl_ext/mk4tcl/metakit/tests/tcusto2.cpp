// tcusto2.cpp -- Regression test program, custom view tests
// $Id: tcusto2.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestCustom2() {
  B(c11, Unique operation, 0);
   {
    c4_IntProp p1("p1"), p2("p2");

    c4_View v1, v2, v3;

    v1.Add(p1[1] + p2[11]);
    v1.Add(p1[1] + p2[22]);
    v1.Add(p1[2] + p2[33]);
    v1.Add(p1[2] + p2[33]);
    v1.Add(p1[3] + p2[44]);
    v1.Add(p1[4] + p2[55]);
    v1.Add(p1[4] + p2[55]);
    v1.Add(p1[4] + p2[55]);

    v2 = v1.Unique();
    A(v2.GetSize() == 5);
    A(p1(v2[0]) == 1);
    A(p1(v2[1]) == 1);
    A(p1(v2[2]) == 2);
    A(p1(v2[3]) == 3);
    A(p1(v2[4]) == 4);

    A(p2(v2[0]) == 11);
    A(p2(v2[1]) == 22);
    A(p2(v2[2]) == 33);
    A(p2(v2[3]) == 44);
    A(p2(v2[4]) == 55);

  }
  E;

  B(c12, Union operation, 0) {
    c4_IntProp p1("p1");

    c4_View v1, v2, v3;

    v1.Add(p1[1]);
    v1.Add(p1[2]);
    v1.Add(p1[3]);

    v2.Add(p1[2]);
    v2.Add(p1[3]);
    v2.Add(p1[4]);
    v2.Add(p1[5]);

    v3 = v1.Union(v2);
    A(v3.GetSize() == 5);
    A(p1(v3[0]) == 1);
    A(p1(v3[1]) == 2);
    A(p1(v3[2]) == 3);
    A(p1(v3[3]) == 4);
    A(p1(v3[4]) == 5);
  }
  E;

  B(c13, Intersect operation, 0) {
    c4_IntProp p1("p1");

    c4_View v1, v2, v3;

    v1.Add(p1[1]);
    v1.Add(p1[2]);
    v1.Add(p1[3]);

    v2.Add(p1[2]);
    v2.Add(p1[3]);
    v2.Add(p1[4]);
    v2.Add(p1[5]);

    v3 = v1.Intersect(v2);
    A(v3.GetSize() == 2);
    A(p1(v3[0]) == 2);
    A(p1(v3[1]) == 3);
  }
  E;

  B(c14, Different operation, 0) {
    c4_IntProp p1("p1");

    c4_View v1, v2, v3;

    v1.Add(p1[1]);
    v1.Add(p1[2]);
    v1.Add(p1[3]);

    v2.Add(p1[2]);
    v2.Add(p1[3]);
    v2.Add(p1[4]);
    v2.Add(p1[5]);

    v3 = v1.Different(v2);
    A(v3.GetSize() == 3);
    A(p1(v3[0]) == 1);
    A(p1(v3[1]) == 4);
    A(p1(v3[2]) == 5);
  }
  E;

  B(c15, Minus operation, 0) {
    c4_IntProp p1("p1");

    c4_View v1, v2, v3;

    v1.Add(p1[1]);
    v1.Add(p1[2]);
    v1.Add(p1[3]);

    v2.Add(p1[2]);
    v2.Add(p1[3]);
    v2.Add(p1[4]);
    v2.Add(p1[5]);

    v3 = v1.Minus(v2);
    A(v3.GetSize() == 1);
    A(p1(v3[0]) == 1);
  }
  E;

  B(c16, View comparisons, 0) {
    c4_IntProp p1("p1");

    c4_View v1;
    v1.Add(p1[1]);
    v1.Add(p1[2]);
    v1.Add(p1[3]);
    v1.Add(p1[4]);
    v1.Add(p1[5]);

    A(v1 == v1);
    A(v1 == v1.Slice(0));
    A(v1.Slice(0, 2) < v1.Slice(0, 3));
    A(v1.Slice(0, 3) == v1.Slice(0, 3));
    A(v1.Slice(0, 4) > v1.Slice(0, 3));
    A(v1.Slice(0, 3) < v1.Slice(1, 3));
    A(v1.Slice(0, 3) < v1.Slice(1, 4));
    A(v1.Slice(1, 3) > v1.Slice(0, 3));
    A(v1.Slice(1, 4) > v1.Slice(0, 3));
  }
  E;

  B(c17, Join operation, 0) {
    c4_StringProp p1("p1"), p2("p2");
    c4_IntProp p3("p3");

    c4_View v1, v2, v3;

    v1.Add(p1[""]);
    v1.Add(p1["1"] + p2["a"]);
    v1.Add(p1["12"] + p2["ab"]);
    v1.Add(p1["123"] + p2["abc"]);

    v2.Add(p1["1"] + p3[1]);
    v2.Add(p1["12"] + p3[1]);
    v2.Add(p1["12"] + p3[2]);
    v2.Add(p1["123"] + p3[1]);
    v2.Add(p1["123"] + p3[2]);
    v2.Add(p1["123"] + p3[3]);

    v3 = v1.Join(p1, v2); // inner join
    A(v3.GetSize() == 6);

    A(p1(v3[0]) == (c4_String)"1");
    A(p1(v3[1]) == (c4_String)"12");
    A(p1(v3[2]) == (c4_String)"12");
    A(p1(v3[3]) == (c4_String)"123");
    A(p1(v3[4]) == (c4_String)"123");
    A(p1(v3[5]) == (c4_String)"123");

    A(p2(v3[0]) == (c4_String)"a");
    A(p2(v3[1]) == (c4_String)"ab");
    A(p2(v3[2]) == (c4_String)"ab");
    A(p2(v3[3]) == (c4_String)"abc");
    A(p2(v3[4]) == (c4_String)"abc");
    A(p2(v3[5]) == (c4_String)"abc");

    A(p3(v3[0]) == 1);
    A(p3(v3[1]) == 1);
    A(p3(v3[2]) == 2);
    A(p3(v3[3]) == 1);
    A(p3(v3[4]) == 2);
    A(p3(v3[5]) == 3);

    v3 = v1.Join(p1, v2, true); // outer join
    A(v3.GetSize() == 7);

    A(p1(v3[0]) == (c4_String)"");
    A(p1(v3[1]) == (c4_String)"1");
    A(p1(v3[2]) == (c4_String)"12");
    A(p1(v3[3]) == (c4_String)"12");
    A(p1(v3[4]) == (c4_String)"123");
    A(p1(v3[5]) == (c4_String)"123");
    A(p1(v3[6]) == (c4_String)"123");

    A(p2(v3[0]) == (c4_String)"");
    A(p2(v3[1]) == (c4_String)"a");
    A(p2(v3[2]) == (c4_String)"ab");
    A(p2(v3[3]) == (c4_String)"ab");
    A(p2(v3[4]) == (c4_String)"abc");
    A(p2(v3[5]) == (c4_String)"abc");
    A(p2(v3[6]) == (c4_String)"abc");

    A(p3(v3[0]) == 0);
    A(p3(v3[1]) == 1);
    A(p3(v3[2]) == 1);
    A(p3(v3[3]) == 2);
    A(p3(v3[4]) == 1);
    A(p3(v3[5]) == 2);
    A(p3(v3[6]) == 3);
  }
  E;

  B(c18, Groupby sort fix, 0) { // fails in 1.8.4 (from P. Ritter, 14-10-1998)
    c4_StringProp p1("Country");
    c4_StringProp p2("City");
    c4_ViewProp p3("SubList");

    c4_View v1, v2, v3;

    v1.Add(p1["US"] + p2["Philadelphia"]);
    v1.Add(p1["France"] + p2["Bordeaux"]);
    v1.Add(p1["US"] + p2["Miami"]);
    v1.Add(p1["France"] + p2["Paris"]);
    v1.Add(p1["US"] + p2["Boston"]);
    v1.Add(p1["France"] + p2["Nice"]);
    v1.Add(p1["US"] + p2["NY"]);
    v1.Add(p1["US"] + p2["Miami"]);

    v2 = v1.GroupBy(p1, p3);
    A(v2.GetSize() == 2);
    A(p1(v2[0]) == (c4_String)"France");
    A(p1(v2[1]) == (c4_String)"US");

    v3 = p3(v2[0]);
    A(v3.GetSize() == 3);
    A(p2(v3[0]) == (c4_String)"Bordeaux");
    A(p2(v3[1]) == (c4_String)"Nice");
    A(p2(v3[2]) == (c4_String)"Paris");
    v3 = p3(v2[1]);
    A(v3.GetSize() == 5);
    A(p2(v3[0]) == (c4_String)"Boston");
    A(p2(v3[1]) == (c4_String)"Miami");
    A(p2(v3[2]) == (c4_String)"Miami");
    A(p2(v3[3]) == (c4_String)"NY");
    A(p2(v3[4]) == (c4_String)"Philadelphia");
  }
  E;

  B(c19, JoinProp operation, 0) { // moved, used to also be called c15
    c4_StringProp p1("p1");
    c4_ViewProp p2("p2");
    c4_IntProp p3("p3");

    c4_View v1, v2a, v2b, v2c, v3;

    v2a.Add(p3[1]);
    v2a.Add(p3[2]);
    v2a.Add(p3[3]);
    v1.Add(p1["123"] + p2[v2a]);

    v2b.Add(p3[1]);
    v2b.Add(p3[2]);
    v1.Add(p1["12"] + p2[v2b]);

    v2c.Add(p3[1]);
    v1.Add(p1["1"] + p2[v2c]);

    v1.Add(p1[""]);

    v3 = v1.JoinProp(p2); // inner join
    A(v3.GetSize() == 6);

    A(p1(v3[0]) == (c4_String)"123");
    A(p1(v3[1]) == (c4_String)"123");
    A(p1(v3[2]) == (c4_String)"123");
    A(p1(v3[3]) == (c4_String)"12");
    A(p1(v3[4]) == (c4_String)"12");
    A(p1(v3[5]) == (c4_String)"1");

    A(p3(v3[0]) == 1);
    A(p3(v3[1]) == 2);
    A(p3(v3[2]) == 3);
    A(p3(v3[3]) == 1);
    A(p3(v3[4]) == 2);
    A(p3(v3[5]) == 1);

    v3 = v1.JoinProp(p2, true); // outer join
    A(v3.GetSize() == 7);

    A(p1(v3[0]) == (c4_String)"123");
    A(p1(v3[1]) == (c4_String)"123");
    A(p1(v3[2]) == (c4_String)"123");
    A(p1(v3[3]) == (c4_String)"12");
    A(p1(v3[4]) == (c4_String)"12");
    A(p1(v3[5]) == (c4_String)"1");
    A(p1(v3[6]) == (c4_String)"");

    A(p3(v3[0]) == 1);
    A(p3(v3[1]) == 2);
    A(p3(v3[2]) == 3);
    A(p3(v3[3]) == 1);
    A(p3(v3[4]) == 2);
    A(p3(v3[5]) == 1);
    A(p3(v3[6]) == 0);
  }
  E;

  B(c20, Wide cartesian product, 0) {
    // added 2nd prop's to do a better test - 1999-12-23
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");
    c4_IntProp p3("p3");
    c4_IntProp p4("p4");

    c4_View v1;
    v1.Add(p1[123] + p2[321]);
    v1.Add(p1[234] + p2[432]);
    v1.Add(p1[345] + p2[543]);

    c4_View v2;
    v2.Add(p3[111] + p4[11]);
    v2.Add(p3[222] + p4[22]);

    c4_View v3 = v1.Product(v2);
    A(v3.GetSize() == 6);
    A(p1(v3[0]) == 123);
    A(p2(v3[0]) == 321);
    A(p3(v3[0]) == 111);
    A(p4(v3[0]) == 11);
    A(p1(v3[1]) == 123);
    A(p2(v3[1]) == 321);
    A(p3(v3[1]) == 222);
    A(p4(v3[1]) == 22);
    A(p1(v3[2]) == 234);
    A(p2(v3[2]) == 432);
    A(p3(v3[2]) == 111);
    A(p4(v3[2]) == 11);
    A(p1(v3[3]) == 234);
    A(p2(v3[3]) == 432);
    A(p3(v3[3]) == 222);
    A(p4(v3[3]) == 22);
    A(p1(v3[4]) == 345);
    A(p2(v3[4]) == 543);
    A(p3(v3[4]) == 111);
    A(p4(v3[4]) == 11);
    A(p1(v3[5]) == 345);
    A(p2(v3[5]) == 543);
    A(p3(v3[5]) == 222);
    A(p4(v3[5]) == 22);

    v1.Add(p1[456]);
    A(v3.GetSize() == 8);
    v2.Add(p2[333]);
    A(v3.GetSize() == 12);
  }
  E;

  B(c21, Join on compound key, 0) {
    c4_IntProp p1("p1"), p2("p2"), p3("p3"), p4("p4");

    c4_View v1, v2, v3;

    v1.Add(p1[1] + p2[11] + p3[111]);
    v1.Add(p1[2] + p2[22] + p3[222]);
    v1.Add(p1[3] + p2[22] + p3[111]);

    v2.Add(p2[11] + p3[111] + p4[1111]);
    v2.Add(p2[22] + p3[222] + p4[2222]);
    v2.Add(p2[22] + p3[222] + p4[3333]);
    v2.Add(p2[22] + p3[333] + p4[4444]);

    // this works here, but it fails in Python, i.e. Mk4py 2.4.0
    v3 = v1.Join((p2, p3), v2);

    A(v3.GetSize() == 3);

    A(p1(v3[0]) == 1);
    A(p1(v3[1]) == 2);
    A(p1(v3[2]) == 2);

    A(p2(v3[0]) == 11);
    A(p2(v3[1]) == 22);
    A(p2(v3[2]) == 22);

    A(p3(v3[0]) == 111);
    A(p3(v3[1]) == 222);
    A(p3(v3[2]) == 222);

    A(p4(v3[0]) == 1111);
    A(p4(v3[1]) == 2222);
    A(p4(v3[2]) == 3333);
  }
  E;

  B(c22, Groupby with selection, 0) {
    c4_Storage s1;
    c4_View v1 = s1.GetAs("v1[p1:I,p2:I,p3:I]");
    c4_IntProp p1("p1"), p2("p2"), p3("p3");
    c4_ViewProp p4("p4");

    v1.Add(p1[0] + p2[1] + p3[10]);
    v1.Add(p1[1] + p2[1] + p3[20]);
    v1.Add(p1[2] + p2[2] + p3[30]);
    v1.Add(p1[3] + p2[3] + p3[40]);
    v1.Add(p1[4] + p2[3] + p3[50]);

    s1.Commit();
    A(v1.GetSize() == 5);

    c4_View v2 = v1.GroupBy(p2, p4);
    A(v2.GetSize() == 3);

    c4_View v3 = p4(v2[0]);
    A(v3.GetSize() == 2);
    A(p3(v3[0]) == 10);
    A(p3(v3[1]) == 20);

    c4_View v4 = p4(v2[1]);
    A(v4.GetSize() == 1);
    A(p3(v4[0]) == 30);

    c4_View v5 = p4(v2[2]);
    A(v5.GetSize() == 2);
    A(p3(v5[0]) == 40);
    A(p3(v5[1]) == 50);

    c4_View v6 = v4.Sort();
    A(v6.GetSize() == 1);
    A(p1(v6[0]) == 2);
    A(p3(v6[0]) == 30);

  }
  E;
}
