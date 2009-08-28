// tformat.cpp -- Regression test program, (re-)format tests
// $Id: tformat.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html
 
#include "regress.h"

void TestFormat() {
  B(f01, Add view to format, 0)W(f01a);
   {
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");
     {
      c4_Storage s1("f01a", 1);
      s1.SetStructure("a[p1:I]");

      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      s1.Commit();

      c4_View v2 = s1.GetAs("b[p2:I]");

      v2.Add(p2[345]);
      v2.Add(p2[567]);

      s1.Commit();
    }
     {
      c4_Storage s1("f01a", 0);

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);

      c4_View v2 = s1.View("b");
      A(v2.GetSize() == 2);
      A(p2(v2[0]) == 345);
      A(p2(v2[1]) == 567);
    }
  }
  D(f01a);
  R(f01a);
  E;

  B(f02, Remove view from format, 0)W(f02a);
   {
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");
     {
      c4_Storage s1("f02a", 1);
      s1.SetStructure("a[p1:I],b[p2:I]");

      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);

      c4_View v2 = s1.View("b");
      v2.Add(p2[345]);
      v2.Add(p2[567]);

      s1.Commit();
    }
     {
      c4_Storage s1("f02a", 1);
      s1.SetStructure("b[p2:I]");

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1); // 19990916 new semantics, still as temp view
      A(p1(v1[0]) == 123);

      c4_View v2 = s1.View("b");
      A(v2.GetSize() == 2);
      A(p2(v2[0]) == 345);
      A(p2(v2[1]) == 567);

      s1.Commit();
    }
     {
      c4_Storage s1("f02a", 0);

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 0);

      c4_View v2 = s1.View("b");
      A(v2.GetSize() == 2);
      A(p2(v2[0]) == 345);
      A(p2(v2[1]) == 567);
    }
  }
  D(f02a);
  R(f02a);
  E;

  B(f03, Rollback format change, 0)W(f03a);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("f03a", 1);
      s1.SetStructure("a[p1:I]");

      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);

      s1.Commit();

      v1 = s1.GetAs("a");
      A(v1.GetSize() == 0);

      s1.Rollback();

      v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);
    }
  }
  D(f03a);
  R(f03a);
  E;

  B(f04, Rearrange format, 0)W(f04a);
   {
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");
     {
      c4_Storage s1("f04a", 1);
      s1.SetStructure("a[p1:I],b[p2:I]");

      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);

      c4_View v2 = s1.View("b");
      v2.Add(p2[345]);
      v2.Add(p2[567]);

      s1.Commit();
    }
     {
      c4_Storage s1("f04a", 1);
      s1.SetStructure("b[p2:I],a[p1:I]");

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);

      c4_View v2 = s1.View("b");
      A(v2.GetSize() == 2);
      A(p2(v2[0]) == 345);
      A(p2(v2[1]) == 567);

      s1.Commit();
    }
  }
  D(f04a);
  R(f04a);
  E;

  B(f05, Nested reformat, 0)W(f05a);
   {
    c4_IntProp p1("p1");
    c4_IntProp p2("p2");
     {
      c4_Storage s1("f05a", 1);
      s1.SetStructure("a[p1:I],b[p2:I]");

      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);

      c4_View v2 = s1.View("b");
      v2.Add(p2[345]);
      v2.Add(p2[567]);

      s1.Commit();
    }
     {
      c4_Storage s1("f05a", 1);
      s1.SetStructure("a[p1:I],b[p1:I,p2:I]");

      c4_View v2 = s1.View("b");
      p1(v2[0]) = 543;
      p1(v2[1]) = 765;

      s1.Commit();
    }
     {
      c4_Storage s1("f05a", 0);

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);

      c4_View v2 = s1.View("b");
      A(v2.GetSize() == 2);
      A(p1(v2[0]) == 543);
      A(p1(v2[1]) == 765);
      A(p2(v2[0]) == 345);
      A(p2(v2[1]) == 567);
    }
  }
  D(f05a);
  R(f05a);
  E;

  B(f06, Flip foreign data, 0) {
    D(reversed); // not created here, only dump existing file
  }
  E;

  B(f07, Automatic structure info (obsolete), 0)W(f07a);
   {
    /* Structure() and Store() are no longer supported
    c4_StringProp p1 ("p1"), p2 ("p2");
    c4_Row r1 = p1 ["One"] + p2 ["Two"];
    c4_Row r2;
    c4_View v1;
    v1.Add(r1);
    v1.Add(r2);
    v1.Add(r1);

    c4_View v2 = v1.Structure();
    A(v2.GetSize() == 1);

    c4_ViewProp pView ("view");
    c4_View v3 = pView (v2[0]);
    A(v3.GetSize() == 2);
     */
#define FORMAT07 "dict[parent:I,index:I,view[name:S,type:S,child:I]]"
    c4_Storage s1("f07a", 1);
    s1.SetStructure(FORMAT07);

    //s1.View("dict") = v1.Structure();

    s1.Commit();

  }
  D(f07a);
  R(f07a);
  E;

  B(f08, Automatic storage format, 0)W(f08a);
   {
    c4_StringProp p1("p1"), p2("p2");
    c4_Row r1 = p1["One"] + p2["Two"];
    c4_Row r2;
    c4_View v1;
    v1.Add(r1);
    v1.Add(r2);
    v1.Add(r1);

    c4_Storage s1("f08a", 1);

    // changed 2000-03-15: Store is gone
    //s1.Store("dict", v1);
    c4_View v2 = s1.GetAs("dict[p1:S,p2:S]");
    v2.InsertAt(0, v1);

    s1.Commit();

  }
  D(f08a);
  R(f08a);
  E;

  B(f09, Partial restructuring, 0)W(f09a);
   {
    c4_IntProp p1("p1"), p2("p2"), p3("p3");
    c4_Storage s1("f09a", 1);

    c4_View v1 = s1.GetAs("a[p1:I]");
    v1.SetSize(10);

    for (int i = 0; i < v1.GetSize(); ++i)
      p1(v1[i]) = 1000+i;

    c4_View v2 = s1.GetAs("a[p1:I,p2:I]");

    for (int j = 0; j < v2.GetSize(); j += 2)
      p2(v2[j]) = 2000+j;

    c4_View v3 = s1.GetAs("a[p1:I,p2:I,p3:I]");

    for (int k = 0; k < v3.GetSize(); k += 3)
      p3(v3[k]) = 3000+k;

    s1.Commit();

  }
  D(f09a);
  R(f09a);
  E;

  B(f10, Committed restructuring, 0)W(f10a);
   {
    c4_IntProp p1("p1"), p2("p2"), p3("p3");
    c4_Storage s1("f10a", 1);

    c4_View v1 = s1.GetAs("a[p1:I]");
    v1.SetSize(10);

    for (int i = 0; i < v1.GetSize(); ++i)
      p1(v1[i]) = 1000+i;

    s1.Commit();

    c4_View v2 = s1.GetAs("a[p1:I,p2:I]");

    for (int j = 0; j < v2.GetSize(); j += 2)
      p2(v2[j]) = 2000+j;

    s1.Commit();

    c4_View v3 = s1.GetAs("a[p1:I,p2:I,p3:I]");

    for (int k = 0; k < v3.GetSize(); k += 3)
      p3(v3[k]) = 3000+k;

    s1.Commit();

  }
  D(f10a);
  R(f10a);
  E;

  // 19990824: don't crash on GetAs with an inexistent view
  B(f11, Delete missing view, 0)W(f11a);
   {
    c4_Storage s1("f11a", 1);

    c4_View v1 = s1.GetAs("a");
    v1.SetSize(10);

    s1.Commit();

  }
  D(f11a);
  R(f11a);
  E;
}
