// tnotify.cpp -- Regression test program, notification tests
// $Id: tnotify.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestNotify() {
  B(n01, Add to selection, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    A(v1.GetSize() == 6);
    c4_View v2 = v1.SelectRange(p1[200], p1[333]);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
    v1.Add(p1[300]);
    A(v1.GetSize() == 7);
    A(v2.GetSize() == 4);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
    A(p1(v2[3]) == 300);
    v1.Add(p1[199]);
    A(v1.GetSize() == 8);
    A(v2.GetSize() == 4);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
    A(p1(v2[3]) == 300);
  }
  E;

  B(n02, Remove from selection, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    A(v1.GetSize() == 6);
    c4_View v2 = v1.SelectRange(p1[200], p1[333]);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
    v1.RemoveAt(2);
    A(v1.GetSize() == 5);
    A(v2.GetSize() == 2);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 234);
    v1.RemoveAt(2);
    A(v1.GetSize() == 4);
    A(v2.GetSize() == 2);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 234);
  }
  E;

  B(n03, Modify into selection, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    A(v1.GetSize() == 6);
    c4_View v2 = v1.SelectRange(p1[200], p1[333]);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
    p1(v1[5]) = 300;
    A(v2.GetSize() == 4);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
    A(p1(v2[3]) == 300);
  }
  E;

  B(n04, Modify out of selection, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    A(v1.GetSize() == 6);
    c4_View v2 = v1.SelectRange(p1[200], p1[333]);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);
    p1(v1[2]) = 100;
    A(v2.GetSize() == 2);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 234);
  }
  E;

  B(n05, Add to sorted, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    A(v1.GetSize() == 6);
    c4_View v2 = v1.Sort();
    A(v2.GetSize() == 6);
    A(p1(v2[0]) == 111);
    A(p1(v2[1]) == 123);
    A(p1(v2[2]) == 222);
    A(p1(v2[3]) == 234);
    A(p1(v2[4]) == 333);
    A(p1(v2[5]) == 345);
    v1.Add(p1[300]);
    A(v2.GetSize() == 7);
    A(p1(v2[0]) == 111);
    A(p1(v2[1]) == 123);
    A(p1(v2[2]) == 222);
    A(p1(v2[3]) == 234);
    A(p1(v2[4]) == 300);
    A(p1(v2[5]) == 333);
    A(p1(v2[6]) == 345);
  }
  E;

  B(n06, Remove from sorted, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.Add(p1[111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    A(v1.GetSize() == 6);
    c4_View v2 = v1.Sort();
    A(v2.GetSize() == 6);
    A(p1(v2[0]) == 111);
    A(p1(v2[1]) == 123);
    A(p1(v2[2]) == 222);
    A(p1(v2[3]) == 234);
    A(p1(v2[4]) == 333);
    A(p1(v2[5]) == 345);
    v1.RemoveAt(2);
    A(v2.GetSize() == 5);
    A(p1(v2[0]) == 111);
    A(p1(v2[1]) == 123);
    A(p1(v2[2]) == 222);
    A(p1(v2[3]) == 234);
    A(p1(v2[4]) == 345);
  }
  E;

  B(n07, New property through sort, 0) {
    c4_IntProp p1("p1"), p2("p2");
    c4_View v1;
    v1.Add(p1[11]);
    v1.Add(p1[1]);
    v1.Add(p1[111]);
    A(v1.FindProperty(p2.GetId()) < 0);

    c4_View v2 = v1.SortOn(p1);
    A(v2.FindProperty(p2.GetId()) < 0);

    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 1);
    A(p1(v2[1]) == 11);
    A(p1(v2[2]) == 111);

    p2(v1[0]) = 22;
    A(v1.FindProperty(p2.GetId()) == 1);
    A(v2.FindProperty(p2.GetId()) == 1);

    A(p2(v2[1]) == 22);
  }
  E;

  B(n08, Nested project and select, 0) {
    c4_IntProp p1("p1"), p2("p2");
    c4_View v1;
    v1.Add(p1[10] + p2[1]);
    v1.Add(p1[11]);
    v1.Add(p1[12] + p2[1]);
    v1.Add(p1[13]);
    v1.Add(p1[14] + p2[1]);
    v1.Add(p1[15]);
    v1.Add(p1[16] + p2[1]);
    A(v1.GetSize() == 7);

    c4_View v2 = v1.Select(p2[1]);
    A(v2.GetSize() == 4);
    A(p1(v2[0]) == 10);
    A(p1(v2[1]) == 12);
    A(p1(v2[2]) == 14);
    A(p1(v2[3]) == 16);

    c4_View v3 = v2.Project(p1);
    A(v3.GetSize() == 4);
    A(p1(v3[0]) == 10);
    A(p1(v3[1]) == 12);
    A(p1(v3[2]) == 14);
    A(p1(v3[3]) == 16);

    A(p2(v3[0]) == 0);
    A(p2(v3[1]) == 0);
    A(p2(v3[2]) == 0);
    A(p2(v3[3]) == 0);

    /* not yet implemented: setting result of selection 
    p1 (v3[1]) = 123;
    A(p1 (v3[1]) == 123);
    A(p1 (v2[1]) == 123);
    A(p1 (v1[2]) == 123);
     */
  }
  E;

  B(n09, Multiple dependencies, 0) {
    c4_IntProp p1("p1"), p2("p2");
    c4_View v1;
    v1.Add(p1[111] + p2[1111]);
    v1.Add(p1[222]);
    v1.Add(p1[333]);
    v1.Add(p1[345]);
    v1.Add(p1[234]);
    v1.Add(p1[123]);
    A(v1.GetSize() == 6);

    c4_View v2 = v1.SelectRange(p1[200], p1[333]);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 333);
    A(p1(v2[2]) == 234);

    c4_View v3 = v1.SelectRange(p1[340], p1[350]);
    A(v3.GetSize() == 1);
    A(p1(v3[0]) == 345);

    c4_View v4 = v2.SortOn(p1);
    A(v4.GetSize() == 3);
    A(p1(v4[0]) == 222);
    A(p1(v4[1]) == 234);
    A(p1(v4[2]) == 333);

    c4_View v5 = v3.SortOn(p1);
    A(v5.GetSize() == 1);
    A(p1(v5[0]) == 345);

    p1(v1[2]) = 346;

    A(v2.GetSize() == 2);
    A(p1(v2[0]) == 222);
    A(p1(v2[1]) == 234);

    A(v3.GetSize() == 2);
    A(p1(v3[0]) == 346);
    A(p1(v3[1]) == 345);

    A(v4.GetSize() == 2);
    A(p1(v4[0]) == 222);
    A(p1(v4[1]) == 234);

    A(v5.GetSize() == 2);
    A(p1(v5[0]) == 345);
    A(p1(v5[1]) == 346);
  }
  E;

  B(n10, Modify sorted duplicates, 0) {
    c4_IntProp p1("p1");
    c4_View v1;
    v1.SetSize(3);
    p1(v1[0]) = 0;
    c4_View v2 = v1.Sort();
    p1(v1[0]) = 1;
    p1(v1[1]) = 1; // crashed in 1.5, fix in: c4_SortSeq::PosInMap
  }
  E;

  B(n11, Resize compound derived view, 0) {
    c4_IntProp p1("p1"), p2("p2");
    c4_View v1 = (p1, p2);
    c4_View v2 = v1.SelectRange(p2[200], p2[333]);
    c4_View v3 = v2.SortOn(p1);
    A(v2.GetSize() == 0);
    A(v3.GetSize() == 0);
    v1.SetSize(1); // crashed in 1.5, fix in: c4_FilterSeq::Match
    A(v1.GetSize() == 1);
    A(v2.GetSize() == 0);
    A(v3.GetSize() == 0);
    v1[0] = p2[300];
    A(v1.GetSize() == 1);
    A(v2.GetSize() == 1);
    A(v3.GetSize() == 1);
    A(p2(v2[0]) == 300);
    v1.Add(p1[199]);
    A(v1.GetSize() == 2);
    A(v2.GetSize() == 1);
    A(p2(v2[0]) == 300);
  }
  E;

  B(n12, Alter multiply derived view, 0) {
    c4_IntProp p1("p1");
    c4_StringProp p2("p2"), p3("p3");
    c4_View v1 = (p1, p2);
    c4_View v2 = v1.Select(p1[1]);
    c4_View v3 = v2.SortOn(p2);
    c4_View v4 = v1.Select(p1[2]);
    c4_View v5 = v4.SortOn(p2);

    v1.Add(p1[1] + p2["een"] + p3["1"]);
    v1.Add(p1[1] + p2["elf"] + p3["11"]);
    v1.Add(p1[2] + p2["twee"] + p3["2"]);
    v1.Add(p1[2] + p2["twaalf"] + p3["12"]);
    v1.Add(p1[2] + p2["twintig"] + p3["20"]);
    v1.Add(p1[2] + p2["tachtig"] + p3["80"]);

    A(v1.GetSize() == 6);
    A(v2.GetSize() == 2);
    A(v3.GetSize() == 2);
    A(v4.GetSize() == 4);
    A(v5.GetSize() == 4);

    A(p3(v1[2]) == (c4_String)"2");
    A(p3(v4[0]) == (c4_String)"2");

    A(p3(v3[0]) == (c4_String)"1");
    A(p3(v3[1]) == (c4_String)"11");

    A(p3(v5[0]) == (c4_String)"80");
    A(p3(v5[1]) == (c4_String)"12");
    A(p3(v5[2]) == (c4_String)"2");
    A(p3(v5[3]) == (c4_String)"20");

    v1[3] = p1[2] + p2["twaalf"] + p3["12+"];

    A(p3(v3[0]) == (c4_String)"1");
    A(p3(v3[1]) == (c4_String)"11");

    A(p3(v1[3]) == (c4_String)"12+");
    A(p3(v4[1]) == (c4_String)"12+");

    A(p3(v5[0]) == (c4_String)"80");
    A(p3(v5[1]) == (c4_String)"12+");
    A(p3(v5[2]) == (c4_String)"2");
    A(p3(v5[3]) == (c4_String)"20");
  }
  E;

  B(n13, Project without, 0) { // failed in 1.8.4
    c4_IntProp p1("p1"), p2("p2");
    c4_View v1;

    v1.Add(p1[1] + p2[2]);
    int n1 = v1.NumProperties();
    A(n1 == 2);

    c4_View v2 = v1.ProjectWithout(p2);
    int n2 = v2.NumProperties();
    A(n2 == 1);
  }
  E;

  /*
  B(n14, Add to reverse sorted, 0)
  {
  c4_IntProp p1 ("p1"), p2 ("p2");
  c4_View v1;
  v1.Add(p1 [333] + p2 [1]);
  v1.Add(p1 [345] + p2 [1]);
  v1.Add(p1 [234] + p2 [1]);
  v1.Add(p1 [123] + p2 [0]);
  A(v1.GetSize() == 4);
  c4_View v1a = v1.Select(p2 [1]);
  A(v1a.GetSize() == 3);
  c4_View v1b = v1a.SelectRange(p1 [100], p1 [999]);
  A(v1b.GetSize() == 3);
  c4_View v2 = v1b.SortOnReverse(p1, p1);
  A(v2.GetSize() == 3);
  A(p1 (v2[0]) == 345);
  A(p1 (v2[1]) == 333);
  A(p1 (v2[2]) == 234);
  v1.Add(p1 [300] + p2 [1]);
  A(v2.GetSize() == 4);
  A(p1 (v2[0]) == 345);
  A(p1 (v2[1]) == 333);
  A(p1 (v2[2]) == 300);
  A(p1 (v2[3]) == 234);
  v1.Add(p1 [299] + p2 [1]);
  A(v2.GetSize() == 5);
  A(p1 (v2[0]) == 345);
  A(p1 (v2[1]) == 333);
  A(p1 (v2[2]) == 300);
  A(p1 (v2[3]) == 299);
  A(p1 (v2[4]) == 234);
  } E;
   */
  // this failed in 2.4.8, reported by S. Selznick, 2002-11-22
  B(n14, Insert in non-mapped position, 0)W(n14a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("n14a", 1);
    s1.SetStructure("a[p1:I]");
    c4_View v1 = s1.View("a");

    static int intlist[] =  {
      0, 1, 2, 0, 1, 2, 0, 1, 2, 0,  - 1
    };
    for (int c = 0;  - 1 != intlist[c]; c++)
      v1.Add(p1[intlist[c]]);

    A(v1.GetSize() == 10);
    c4_View v2 = v1.Select(p1[1]);
    A(v2.GetSize() == 3);

    v1.InsertAt(3, p1[6]);
    A(v1.GetSize() == 11);
    A(v2.GetSize() == 3);

    v1.InsertAt(7, p1[1]);
    A(v1.GetSize() == 12);
    A(v2.GetSize() == 4);

    s1.Commit();
  }
  D(n14a);
  R(n14a);
  E;
}
