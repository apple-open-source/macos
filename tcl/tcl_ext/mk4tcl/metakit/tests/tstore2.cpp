// tstore2.cpp -- Regression test program, storage tests, part 2
// $Id: tstore2.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestStores2() {
  B(s10, Stream storage, 0)W(s10a);
  W(s10b);
  W(s10c);
   {
    // s10a is original
    // s10b is a copy, random access
    // s10c is a serialized copy
    c4_StringProp p1("p1");
    c4_ViewProp p2("p2");
    c4_IntProp p3("p3");
     {
      c4_Storage s1("s10a", 1);
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
     {
      c4_Storage s1("s10a", 0);
      c4_Storage s2("s10b", 1);
      s2.SetStructure("a[p1:S,p2[p3:I]]");
      s2.View("a") = s1.View("a");
      s2.Commit();
    }
     {
      c4_Storage s3("s10b", 0);

      c4_FileStream fs1(fopen("s10c", "wb"), true);
      s3.SaveTo(fs1);
    }
     {
      c4_Storage s1("s10c", 0); 
        // new after 2.01: serialized is no longer special

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 3);
      c4_View v2 = p2(v1[0]);
      A(v2.GetSize() == 1);
      c4_View v3 = p2(v1[1]);
      A(v3.GetSize() == 3);
      c4_View v4 = p2(v1[2]);
      A(v4.GetSize() == 2);
    }
     {
      c4_Storage s1;

      c4_FileStream fs1(fopen("s10c", "rb"), true);
      s1.LoadFrom(fs1);

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 3);
      c4_View v2 = p2(v1[0]);
      A(v2.GetSize() == 1);
      c4_View v3 = p2(v1[1]);
      A(v3.GetSize() == 3);
      c4_View v4 = p2(v1[2]);
      A(v4.GetSize() == 2);
    }
     {
      c4_Storage s1("s10c", 1);

      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 3);
      c4_View v2 = p2(v1[0]);
      A(v2.GetSize() == 1);
      c4_View v3 = p2(v1[1]);
      A(v3.GetSize() == 3);
      c4_View v4 = p2(v1[2]);
      A(v4.GetSize() == 2);
      v1.Add(p1["four"]);
      s1.Commit();
    }
     {
      c4_Storage s1("s10c", 0);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 4);
      c4_View v2 = p2(v1[0]);
      A(v2.GetSize() == 1);
      c4_View v3 = p2(v1[1]);
      A(v3.GetSize() == 3);
      c4_View v4 = p2(v1[2]);
      A(v4.GetSize() == 2);
      c4_View v5 = p2(v1[3]);
      A(v5.GetSize() == 0);
    }
  }
  D(s10a);
  D(s10b);
  D(s10c);
  R(s10a);
  R(s10b);
  R(s10c);
  E;

  B(s11, Commit and rollback, 0)W(s11a);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s11a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      s1.Commit();
    }
     {
      c4_Storage s1("s11a", 0);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);
      v1.InsertAt(0, p1[234]);
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 234);
      A(p1(v1[1]) == 123);
      s1.Rollback();
      // 19990916 - semantics changed, still 2 rows, but 0 props
      A(v1.GetSize() == 2);
      A(v1.NumProperties() == 0);
      v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);
    }
  }
  D(s11a);
  R(s11a);
  E;

  B(s12, Remove subview, 0)W(s12a);
   {
    c4_IntProp p1("p1"), p3("p3");
    c4_ViewProp p2("p2");
     {
      c4_Storage s1("s12a", 1);
      s1.SetStructure("a[p1:I,p2[p3:I]]");
      c4_View v1 = s1.View("a");
      c4_View v2;
      v2.Add(p3[234]);
      v1.Add(p1[123] + p2[v2]);
      s1.Commit();
    }
     {
      c4_Storage s1("s12a", 1);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 123);
      c4_View v2 = p2(v1[0]);
      A(v2.GetSize() == 1);
      A(p3(v2[0]) == 234);
      v1.RemoveAt(0);
      A(v1.GetSize() == 0);
      s1.Commit();
      A(v1.GetSize() == 0);
    }
  }
  D(s12a);
  R(s12a);
  E;

  B(s13, Remove middle subview, 0)W(s13a);
   {
    c4_IntProp p1("p1"), p3("p3");
    c4_ViewProp p2("p2");
     {
      c4_Storage s1("s13a", 1);
      s1.SetStructure("a[p1:I,p2[p3:I]]");
      c4_View v1 = s1.View("a");

      c4_View v2a;
      v2a.Add(p3[234]);
      v1.Add(p1[123] + p2[v2a]);

      c4_View v2b;
      v2b.Add(p3[345]);
      v2b.Add(p3[346]);
      v1.Add(p1[124] + p2[v2b]);

      c4_View v2c;
      v2c.Add(p3[456]);
      v2c.Add(p3[457]);
      v2c.Add(p3[458]);
      v1.Add(p1[125] + p2[v2c]);

      s1.Commit();
    }
     {
      c4_Storage s1("s13a", 1);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 3);
      A(p1(v1[0]) == 123);
      A(p1(v1[1]) == 124);
      A(p1(v1[2]) == 125);
      c4_View v2a = p2(v1[0]);
      A(v2a.GetSize() == 1);
      A(p3(v2a[0]) == 234);
      c4_View v2b = p2(v1[1]);
      A(v2b.GetSize() == 2);
      A(p3(v2b[0]) == 345);
      c4_View v2c = p2(v1[2]);
      A(v2c.GetSize() == 3);
      A(p3(v2c[0]) == 456);
      v1.RemoveAt(1);
      A(v1.GetSize() == 2);
      v2a = p2(v1[0]);
      A(v2a.GetSize() == 1);
      A(p3(v2a[0]) == 234);
      v2b = p2(v1[1]);
      A(v2b.GetSize() == 3);
      A(p3(v2b[0]) == 456);
      s1.Commit();
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 123);
      A(p1(v1[1]) == 125);
    }
  }
  D(s13a);
  R(s13a);
  E;

  B(s14, Replace attached subview, 0)W(s14a);
   {
    c4_IntProp p1("p1");
    c4_ViewProp p2("p2");
     {
      c4_Storage s1("s14a", 1);
      s1.SetStructure("a[p1:I,p2[p3:I]]");
      c4_View v1 = s1.View("a");

      v1.Add(p1[123] + p2[c4_View()]);
      A(v1.GetSize() == 1);

      v1[0] = p2[c4_View()];
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 0);

      s1.Commit();
    }
  }
  D(s14a);
  R(s14a);
  E;

  B(s15, Add after removed subviews, 0)W(s15a);
   {
    c4_IntProp p1("p1"), p3("p3");
    c4_ViewProp p2("p2");
     {
      c4_Storage s1("s15a", 1);
      s1.SetStructure("a[p1:I,p2[p3:I]]");
      c4_View v1 = s1.View("a");

      c4_View v2;
      v2.Add(p3[234]);

      v1.Add(p1[123] + p2[v2]);
      v1.Add(p1[456] + p2[v2]);
      v1.Add(p1[789] + p2[v2]);
      A(v1.GetSize() == 3);

      v1[0] = v1[2];
      v1.RemoveAt(2);

      v1[0] = v1[1];
      v1.RemoveAt(1);

      v1.RemoveAt(0);

      v1.Add(p1[111] + p2[v2]);

      s1.Commit();
    }
  }
  D(s15a);
  R(s15a);
  E;

  B(s16, Add after removed ints, 0)W(s16a);
   {
    c4_IntProp p1("p1");

    c4_Storage s1("s16a", 1);
    s1.SetStructure("a[p1:I,p2[p3:I]]");
    c4_View v1 = s1.View("a");

    v1.Add(p1[1]);
    v1.Add(p1[2]);
    v1.Add(p1[3]);

    v1.RemoveAt(2);
    v1.RemoveAt(1);
    v1.RemoveAt(0);

    v1.Add(p1[4]);

    s1.Commit();

  }
  D(s16a);
  R(s16a);
  E;

  B(s17, Add after removed strings, 0)W(s17a);
   {
    c4_StringProp p1("p1");

    c4_Storage s1("s17a", 1);
    s1.SetStructure("a[p1:S,p2[p3:I]]");
    c4_View v1 = s1.View("a");

    v1.Add(p1["one"]);
    v1.Add(p1["two"]);
    v1.Add(p1["three"]);

    v1.RemoveAt(2);
    v1.RemoveAt(1);
    v1.RemoveAt(0);

    v1.Add(p1["four"]);

    s1.Commit();

  }
  D(s17a);
  R(s17a);
  E;

  B(s18, Empty storage, 0)W(s18a);
   {
    c4_Storage s1("s18a", 1);

  }
  D(s18a);
  R(s18a);
  E;

  B(s19, Empty view outlives storage, 0)W(s19a);
   {
    c4_View v1;
    c4_Storage s1("s19a", 1);
    v1 = s1.GetAs("a[p1:I,p2:S]");

  }
  D(s19a);
  R(s19a);
  E;
}
