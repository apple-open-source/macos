// tstore3.cpp -- Regression test program, storage tests, part 3
// $Id: tstore3.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestStores3() {
  B(s20, View outlives storage, 0)W(s20a);
   {
    c4_IntProp p1("p1");
    c4_View v1;

     {
      c4_Storage s1("s20a", 1);
      v1 = s1.GetAs("a[p1:I,p2:S]");
      v1.Add(p1[123]);
    }

    // 19990916 - semantics changed, rows kept but no properties
    //A(p1 (v1[0]) == 123);
    A(v1.GetSize() == 1);
    A(v1.NumProperties() == 0);

  }
  D(s20a);
  R(s20a);
  E;

  B(s21, Test demo scenario, 0)W(s21a);
   {
    c4_StringProp p1("p1"), p2("p2");
     {
      c4_Storage storage("s21a", 1);
      storage.SetStructure("a[p1:S,p2:S]");
      c4_View v1;
      c4_Row r1;

      p1(r1) = "One";
      p2(r1) = "Un";
      v1.Add(r1);
      A(v1.GetSize() == 1);

      p1(r1) = "Two";
      p2(r1) = "Deux";
      v1.Add(r1);
      A(v1.GetSize() == 2);

      // changed 2000-03-15: Store is gone
      //v1 = storage.Store("a", v1);
      v1 = storage.View("a") = v1;

      A(v1.GetSize() == 2);
      A(p1(v1[1]) == (c4_String)"Two");
      A(p2(v1[1]) == (c4_String)"Deux");
      A(p1(v1[0]) == (c4_String)"One");
      A(p2(v1[0]) == (c4_String)"Un");

      storage.Commit();
      A(v1.GetSize() == 2);
      A(p1(v1[1]) == (c4_String)"Two");
      A(p2(v1[1]) == (c4_String)"Deux");
      A(p1(v1[0]) == (c4_String)"One");
      A(p2(v1[0]) == (c4_String)"Un");

      c4_String s1(p1(v1[1]));
      c4_String s2(p2(v1[1]));
      A(s1 == "Two");
      A(s2 == "Deux");

      storage.Commit();

      v1.Add(p1["Three"] + p2["Trois"]);

      storage.Commit();
      A(v1.GetSize() == 3);
      A(p2(v1[2]) == (c4_String)"Trois");

      v1 = storage.GetAs("a[p1:S,p2:S,p3:I]");
      A(v1.GetSize() == 3);
      A(p2(v1[2]) == (c4_String)"Trois");

      c4_IntProp p3("p3");
      p3(v1[1]) = 123;

      storage.Commit();
      A(v1.GetSize() == 3);
      A(p2(v1[2]) == (c4_String)"Trois");

      c4_View v2 = storage.GetAs("b[p4:I]");

      c4_IntProp p4("p4");
      v2.Add(p4[234]);

      storage.Commit();
      A(v1.GetSize() == 3);
      A(p2(v1[2]) == (c4_String)"Trois");

      c4_IntProp p4a("p4");
      v1.InsertAt(2, p1["Four"] + p4a[345]);

      storage.Commit();
      A(v1.GetSize() == 4);
      A(p1(v1[0]) == (c4_String)"One");
      A(p1(v1[1]) == (c4_String)"Two");
      A(p1(v1[2]) == (c4_String)"Four");
      A(p1(v1[3]) == (c4_String)"Three");
      A(p2(v1[3]) == (c4_String)"Trois");
      A(v2.GetSize() == 1);
      A(p4(v2[0]) == 234);
    }
     {
      c4_Storage storage("s21a", 0);
      c4_View v1 = storage.View("a");
      A(v1.GetSize() == 4);
      A(p1(v1[0]) == (c4_String)"One");
      A(p1(v1[1]) == (c4_String)"Two");
      A(p1(v1[2]) == (c4_String)"Four");
      A(p1(v1[3]) == (c4_String)"Three");
      c4_View v2 = storage.View("b");
      c4_IntProp p4("p4");
      A(v2.GetSize() == 1);
      A(p4(v2[0]) == 234);
    }
  }
  D(s21a);
  R(s21a);
  E;

#if !q4_TINY
  B(s22, Double storage, 0)W(s22a);
   {
    c4_DoubleProp p1("p1");
    c4_Storage s1("s22a", 1);
    s1.SetStructure("a[p1:D]");
    c4_View v1 = s1.View("a");
    v1.Add(p1[1234.5678]);
    v1.Add(p1[2345.6789]);
    v1.InsertAt(1, p1[3456.7890]);
    s1.Commit();
  }
  D(s22a);
  R(s22a);
  E;
#endif 

  B(s23, Find absent record, 0)W(s23a);
   {
    c4_Storage s1("s23a", 1);
    s1.SetStructure("v[h:S,p:I,a:I,b:I,c:I,d:I,e:I,f:I,g:I,x:I]");
    c4_View view = s1.View("v");

    c4_StringProp H("h");
    c4_IntProp P("p");

    c4_Row row;
    H(row) = "someString";
    P(row) = 99;

    int x = view.Find(row);
    A(x ==  - 1);

  }
  D(s23a);
  R(s23a);
  E;

  B(s24, Bitwise storage, 0)W(s24a);
   {
    c4_IntProp p1("p1");

    int m = 9;

    // insert values in front, but check fractional sizes at each step
    for (int n = 0; n < m; ++n) {
       {
        c4_Storage s1("s24a", 1);
        s1.SetStructure("a1[p1:I],a2[p1:I],a3[p1:I],a4[p1:I]");
        s1.AutoCommit(); // new feature in 1.6

        c4_View v1 = s1.View("a1");
        c4_View v2 = s1.View("a2");
        c4_View v3 = s1.View("a3");
        c4_View v4 = s1.View("a4");

        c4_Row row;
        int k = ~n;

        p1(row) = k &0x01;
        v1.InsertAt(0, row);

        p1(row) = k &0x03;
        v2.InsertAt(0, row);

        p1(row) = k &0x0F;
        v3.InsertAt(0, row);

        p1(row) = k &0x7F;
        v4.InsertAt(0, row);
      }
      // the following checks that all tiny size combinations work
       {
        c4_Storage s1("s24a", 0);

        c4_View v1 = s1.View("a1");
        c4_View v2 = s1.View("a2");
        c4_View v3 = s1.View("a3");
        c4_View v4 = s1.View("a4");

        A(v1.GetSize() == n + 1);
        A(v2.GetSize() == n + 1);
        A(v3.GetSize() == n + 1);
        A(v4.GetSize() == n + 1);
      }
    }

    c4_Storage s1("s24a", 0);

    c4_View v1 = s1.View("a1");
    c4_View v2 = s1.View("a2");
    c4_View v3 = s1.View("a3");
    c4_View v4 = s1.View("a4");

    A(v1.GetSize() == m);
    A(v2.GetSize() == m);
    A(v3.GetSize() == m);
    A(v4.GetSize() == m);

    // now check that the inserted values are correct
    for (int i = 0; i < m; ++i) {
      int j = m - i - 1;
      int k = ~i;

      A(p1(v1[j]) == (k &0x01));
      A(p1(v2[j]) == (k &0x03));
      A(p1(v3[j]) == (k &0x0F));
      A(p1(v4[j]) == (k &0x7F));
    }

  }
  D(s24a);
  R(s24a);
  E;

  B(s25, Bytes storage, 0)W(s25a);
   {
    c4_Bytes hi("hi", 2);
    c4_Bytes gday("gday", 4);
    c4_Bytes hello("hello", 5);

    c4_BytesProp p1("p1");
    c4_Storage s1("s25a", 1);
    s1.SetStructure("a[p1:B]");
    c4_View v1 = s1.View("a");

    v1.Add(p1[hi]);
    A(p1(v1[0]) == hi);
    v1.Add(p1[hello]);
    A(p1(v1[0]) == hi);
    A(p1(v1[1]) == hello);
    v1.InsertAt(1, p1[gday]);
    A(p1(v1[0]) == hi);
    A(p1(v1[1]) == gday);
    A(p1(v1[2]) == hello);
    s1.Commit();
    A(p1(v1[0]) == hi);
    A(p1(v1[1]) == gday);
    A(p1(v1[2]) == hello);

  }
  D(s25a);
  R(s25a);
  E;

  B(s26, Bitwise autosizing, 0)W(s26a);
   {
    c4_IntProp p1("p1"), p2("p2"), p3("p3"), p4("p4");
    c4_Storage s1("s26a", 1);
    s1.SetStructure("a[p1:I,p2:I,p3:I,p4:I]");
    c4_View v1 = s1.View("a");

    v1.Add(p1[1] + p2[3] + p3[15] + p4[127]);
    A(p1(v1[0]) == 1);
    A(p2(v1[0]) == 3);
    A(p3(v1[0]) == 15);
    A(p4(v1[0]) == 127);

    p1(v1[0]) = 100000L;
    p2(v1[0]) = 100000L;
    p3(v1[0]) = 100000L;
    p4(v1[0]) = 100000L;

    // these failed in 1.61
    A(p1(v1[0]) == 100000L);
    A(p2(v1[0]) == 100000L);
    A(p3(v1[0]) == 100000L);
    A(p4(v1[0]) == 100000L);

    s1.Commit();

  }
  D(s26a);
  R(s26a);
  E;

  B(s27, Bytes restructuring, 0)W(s27a);
   {
    c4_Bytes test("test", 4);

    c4_BytesProp p1("p1");
    c4_Storage s1("s27a", 1);

    c4_Row row;
    p1(row) = test;

    c4_View v1;
    v1.Add(row);

    // changed 2000-03-15: Store is gone
    //s1.Store("a", v1); // asserts in 1.61
    c4_View v2 = s1.GetAs("a[p1:B]");
    v2.InsertAt(0, v1);

    s1.Commit();

  }
  D(s27a);
  R(s27a);
  E;

#if !q4_TINY
  B(s28, Doubles added later, 0)W(s28a);
   {
    c4_FloatProp p1("p1");
    c4_DoubleProp p2("p2");
    c4_ViewProp p3("p3");

    c4_Storage s1("s28a", 1);
    s1.SetStructure("a[p1:F,p2:D,p3[p1:F,p2:D]]");
    c4_View v1 = s1.View("a");

    c4_Row r1;

    p1(r1) = 123;
    p2(r1) = 123;

    c4_View v2;
    v2.Add(p1[234] + p2[234]);
    p3(r1) = v2;

    v1.Add(r1);
    double x1 = p1(v1[0]);
    A(x1 == p2(v1[0]));

    v2 = p3(v1[0]);
    double x2 = p1(v2[0]);
    A(x2 == p2(v2[0])); // fails in 1.6

    s1.Commit();

  }
  D(s28a);
  R(s28a);
  E;
#endif 

  B(s29, Delete bytes property, 0)W(s29a);
   {
     {
      c4_BytesProp p1("p1");

      c4_Storage s1("s29a", 1);
      s1.SetStructure("a[p1:B]");
      c4_View v1 = s1.View("a");

      int data = 99;
      v1.Add(p1[c4_Bytes(&data, sizeof data)]);

      s1.Commit();
    }
     {
      c4_Storage s1("s29a", 1);
      c4_View v1 = s1.View("a");

      v1.RemoveAt(0); // asserts in 1.7

      s1.Commit();
    }

  }
  D(s29a);
  R(s29a);
  E;
}
