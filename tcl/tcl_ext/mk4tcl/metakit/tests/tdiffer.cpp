// tdiffer.cpp -- Regression test program, differential commit tests
// $Id: tdiffer.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestDiffer() {
  B(d01, Commit aside, 0)W(d01a);
  W(d01b);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("d01a", 1);
      A(s1.Strategy().FileSize() == 0);
      c4_View v1 = s1.GetAs("a[p1:I]");
      v1.Add(p1[123]);
      s1.Commit();
    }
     {
      c4_Storage s1("d01a", 0);
      c4_Storage s2("d01b", 1);
      s1.SetAside(s2);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);
      v1.Add(p1[456]);
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 123);
      A(p1(v1[1]) == 456);
      s1.Commit();
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 123);
      A(p1(v1[1]) == 456);
      s2.Commit();
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 123);
      A(p1(v1[1]) == 456);
    }
     {
      c4_Storage s1("d01a", 0);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);
      c4_Storage s2("d01b", 0);
      s1.SetAside(s2);
      c4_View v2 = s1.View("a");
      A(v2.GetSize() == 2);
      A(p1(v2[0]) == 123);
      A(p1(v2[1]) == 456);
    }
  }
  D(d01a);
  D(d01b);
  R(d01a);
  R(d01b);
  E;
}
