// tcusto1.cpp -- Regression test program, custom view tests
// $Id: tcusto1.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestCustom1() {
  B(c01, Slice forward, 0) {
    c4_IntProp p1("p1");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);
    v1.Add(p1[456]);
    v1.Add(p1[567]);

    c4_View v2 = v1.Slice(1,  - 1, 2);
    A(v2.GetSize() == 2);
    A(p1(v2[0]) == 234);
    A(p1(v2[1]) == 456);

    v1.Add(p1[678]);
    A(v1.GetSize() == 6);
    A(v2.GetSize() == 3);
    A(p1(v2[2]) == 678);
  }
  E;

  B(c02, Slice backward, 0) {
    c4_IntProp p1("p1");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);
    v1.Add(p1[456]);
    v1.Add(p1[567]);

    c4_View v2 = v1.Slice(1,  - 1,  - 2);
    A(v2.GetSize() == 2);
    A(p1(v2[0]) == 456);
    A(p1(v2[1]) == 234);

    v1.Add(p1[678]);
    A(v1.GetSize() == 6);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 678);
    A(p1(v2[1]) == 456);
    A(p1(v2[2]) == 234);
  }
  E;

  B(c03, Slice reverse, 0) {
    c4_IntProp p1("p1");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);
    v1.Add(p1[456]);
    v1.Add(p1[567]);

    c4_View v2 = v1.Slice(1, 5,  - 1);
    A(v2.GetSize() == 4);
    A(p1(v2[0]) == 567);
    A(p1(v2[1]) == 456);
    A(p1(v2[2]) == 345);
    A(p1(v2[3]) == 234);

    v1.Add(p1[678]);
    A(v1.GetSize() == 6);
    A(v2.GetSize() == 4);
  }
  E;

  B(c04, Cartesian product, 0) {
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);

    c4_View v2;
    v2.Add(p2[111]);
    v2.Add(p2[222]);

    c4_View v3 = v1.Product(v2);
    A(v3.GetSize() == 6);
    A(p1(v3[0]) == 123);
    A(p2(v3[0]) == 111);
    A(p1(v3[1]) == 123);
    A(p2(v3[1]) == 222);
    A(p1(v3[2]) == 234);
    A(p2(v3[2]) == 111);
    A(p1(v3[3]) == 234);
    A(p2(v3[3]) == 222);
    A(p1(v3[4]) == 345);
    A(p2(v3[4]) == 111);
    A(p1(v3[5]) == 345);
    A(p2(v3[5]) == 222);

    v1.Add(p1[456]);
    A(v3.GetSize() == 8);
    v2.Add(p2[333]);
    A(v3.GetSize() == 12);
  }
  E;

  B(c05, Remapping, 0) {
    c4_IntProp p1("p1");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);

    c4_View v2;
    v2.Add(p1[2]);
    v2.Add(p1[0]);
    v2.Add(p1[1]);
    v2.Add(p1[0]);

    c4_View v3 = v1.RemapWith(v2);
    A(v3.GetSize() == 4);
    A(p1(v3[0]) == 345);
    A(p1(v3[1]) == 123);
    A(p1(v3[2]) == 234);
    A(p1(v3[3]) == 123);
  }
  E;

  B(c06, Pairwise combination, 0) {
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);

    c4_View v2;
    v2.Add(p2[111]);
    v2.Add(p2[222]);
    v2.Add(p2[333]);

    c4_View v3 = v1.Pair(v2);
    A(v3.GetSize() == 3);
    A(p1(v3[0]) == 123);
    A(p2(v3[0]) == 111);
    A(p1(v3[1]) == 234);
    A(p2(v3[1]) == 222);
    A(p1(v3[2]) == 345);
    A(p2(v3[2]) == 333);
  }
  E;

  B(c07, Concatenate views, 0) {
    c4_IntProp p1("p1");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);

    c4_View v2;
    v2.Add(p1[111]);
    v2.Add(p1[222]);

    c4_View v3 = v1.Concat(v2);
    A(v3.GetSize() == 5);
    A(p1(v3[0]) == 123);
    A(p1(v3[1]) == 234);
    A(p1(v3[2]) == 345);
    A(p1(v3[3]) == 111);
    A(p1(v3[4]) == 222);
  }
  E;

  B(c08, Rename property, 0) {
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");

    c4_View v1;
    v1.Add(p1[123]);
    v1.Add(p1[234]);
    v1.Add(p1[345]);

    c4_View v2 = v1.Rename(p1, p2);
    A(v2.GetSize() == 3);
    A(p2(v2[0]) == 123);
    A(p2(v2[1]) == 234);
    A(p2(v2[2]) == 345);
    A(p1(v2[0]) == 0);
    A(p1(v2[1]) == 0);
    A(p1(v2[2]) == 0);
  }
  E;

  B(c09, GroupBy operation, 0) {
    c4_StringProp p1("p1");
    c4_IntProp p2("p2");
    c4_ViewProp p3("p3");

    c4_View v1, v2, v3;

    v1.Add(p1[""]);
    v1.Add(p1["1"] + p2[1]);
    v1.Add(p1["12"] + p2[1]);
    v1.Add(p1["12"] + p2[2]);
    v1.Add(p1["123"] + p2[1]);
    v1.Add(p1["123"] + p2[2]);
    v1.Add(p1["123"] + p2[3]);

    v2 = v1.GroupBy(p1, p3);
    A(v2.GetSize() == 4);
    A(p1(v2[0]) == (c4_String)"");
    A(p1(v2[1]) == (c4_String)"1");
    A(p1(v2[2]) == (c4_String)"12");
    A(p1(v2[3]) == (c4_String)"123");

    v3 = p3(v2[0]);
    A(v3.GetSize() == 1);
    A(p2(v3[0]) == 0);
    v3 = p3(v2[1]);
    A(v3.GetSize() == 1);
    A(p2(v3[0]) == 1);
    v3 = p3(v2[2]);
    A(v3.GetSize() == 2);
    A(p2(v3[0]) == 1);
    A(p2(v3[1]) == 2);
    v3 = p3(v2[3]);
    A(v3.GetSize() == 3);
    A(p2(v3[0]) == 1);
    A(p2(v3[1]) == 2);
    A(p2(v3[2]) == 3);

  }
  E;

  B(c10, Counts operation, 0) {
    c4_StringProp p1("p1");
    c4_IntProp p2("p2"), p3("p3");

    c4_View v1, v2, v3;

    v1.Add(p1[""]);
    v1.Add(p1["1"] + p2[1]);
    v1.Add(p1["12"] + p2[1]);
    v1.Add(p1["12"] + p2[2]);
    v1.Add(p1["123"] + p2[1]);
    v1.Add(p1["123"] + p2[2]);
    v1.Add(p1["123"] + p2[3]);

    v2 = v1.Counts(p1, p3);
    A(v2.GetSize() == 4);
    A(p1(v2[0]) == (c4_String)"");
    A(p1(v2[1]) == (c4_String)"1");
    A(p1(v2[2]) == (c4_String)"12");
    A(p1(v2[3]) == (c4_String)"123");

    A(p2(v2[0]) == 0);
    A(p2(v2[1]) == 0);
    A(p2(v2[2]) == 0);
    A(p2(v2[3]) == 0);

    A(p3(v2[0]) == 1);
    A(p3(v2[1]) == 1);
    A(p3(v2[2]) == 2);
    A(p3(v2[3]) == 3);

  }
  E;
}
