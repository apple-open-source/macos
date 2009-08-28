// tstore1.cpp -- Regression test program, storage tests, part 1
// $Id: tstore1.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestStores1() {
  B(s00, Simple storage, 0)W(s00a);
   {
    c4_Storage s1("s00a", 1);
    s1.SetStructure("a[p1:I]");
    s1.Commit();
  }
  D(s00a);
  R(s00a);
  E;

  B(s01, Integer storage, 0)W(s01a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("s01a", 1);
    s1.SetStructure("a[p1:I]");
    c4_View v1 = s1.View("a");
    v1.Add(p1[123]);
    v1.Add(p1[456]);
    v1.InsertAt(1, p1[789]);
    A(v1.GetSize() == 3);
    s1.Commit();
    A(v1.GetSize() == 3);
  }
  D(s01a);
  R(s01a);
  E;

#if !q4_TINY
  B(s02, Float storage, 0)W(s02a);
   {
    c4_FloatProp p1("p1");
    c4_Storage s1("s02a", 1);
    s1.SetStructure("a[p1:F]");
    c4_View v1 = s1.View("a");
    v1.Add(p1[12.3]);
    v1.Add(p1[45.6]);
    v1.InsertAt(1, p1[78.9]);
    s1.Commit();
  }
  D(s02a);
  R(s02a);
  E;
#endif 

  B(s03, String storage, 0)W(s03a);
   {
    c4_StringProp p1("p1");
    c4_Storage s1("s03a", 1);
    s1.SetStructure("a[p1:S]");
    c4_View v1 = s1.View("a");
    v1.Add(p1["one"]);
    v1.Add(p1["two"]);
    v1.InsertAt(1, p1["three"]);
    s1.Commit();
  }
  D(s03a);
  R(s03a);
  E;

  B(s04, View storage, 0)W(s04a);
   {
    c4_StringProp p1("p1");
    c4_ViewProp p2("p2");
    c4_IntProp p3("p3");
    c4_Storage s1("s04a", 1);
    s1.SetStructure("a[p1:S,p2[p3:I]]");
    c4_View v1 = s1.View("a");
    v1.Add(p1["one"]);
    v1.Add(p1["two"]);
    c4_View v2 = p2(v1[0]);
    v2.Add(p3[1]);
    v2 = p2(v1[1]);
    v2.Add(p3[11]);
    v2.Add(p3[22]);
    v1.InsertAt(1, p1["three"]);
    v2 = p2(v1[1]);
    v2.Add(p3[111]);
    v2.Add(p3[222]);
    v2.Add(p3[333]);
    s1.Commit();
  }
  D(s04a);
  R(s04a);
  E;

  B(s05, Store and reload, 0)W(s05a);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s05a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      s1.Commit();
    }
     {
      c4_Storage s1("s05a", 0);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);
    }
  }
  D(s05a);
  R(s05a);
  E;

  B(s06, Commit twice, 0)W(s06a);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s06a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      s1.Commit();
      v1.Add(p1[234]);
      s1.Commit();
    }
     {
      c4_Storage s1("s06a", 0);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 123);
      A(p1(v1[1]) == 234);
    }
  }
  D(s06a);
  R(s06a);
  E;

  B(s07, Commit modified, 0)W(s07a);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s07a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      s1.Commit();
      p1(v1[0]) = 234;
      s1.Commit();
    }
     {
      c4_Storage s1("s07a", 0);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 234);
    }
  }
  D(s07a);
  R(s07a);
  E;

  B(s08, View after storage, 0)W(s08a);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s08a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      s1.Commit();
    }
    c4_View v1;
     {
      c4_Storage s1("s08a", 0);
      v1 = s1.View("a");
    }
    // 19990916 - semantics changed, view now 1 row, but 0 props
    A(v1.GetSize() == 1);
    A(v1.NumProperties() == 0);
    v1.InsertAt(0, p1[234]);
    A(v1.GetSize() == 2);
    A(p1(v1[0]) == 234);
    A(p1(v1[1]) == 0); // the original value is gone
  }
  D(s08a);
  R(s08a);
  E;

  B(s09, Copy storage, 0)W(s09a);
  W(s09b);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s09a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      s1.Commit();
    }
     {
      c4_Storage s1("s09a", 0);
      c4_Storage s2("s09b", 1);
      s2.SetStructure("a[p1:I]");
      s2.View("a") = s1.View("a");
      s2.Commit();
    }
  }
  D(s09a);
  D(s09b);
  R(s09a);
  R(s09b);
  E;
}
