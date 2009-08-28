// tlimits.cpp -- Regression test program, limit tests
// $Id: tlimits.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestLimits() {
  B(l00, Lots of properties, 0)W(l00a);
   {
    c4_String desc;

    for (int i = 1; i < 150; ++i) {
      char buf[20];
      sprintf(buf, ",p%d:I", i);

      desc += buf;
    }

    desc = "a[" + desc.Mid(1) + "]";

    c4_Storage s1("l00a", 1);
    s1.SetStructure(desc);
    c4_View v1 = s1.View("a");
    c4_IntProp p123("p123");
    v1.Add(p123[123]);
    s1.Commit();

  }
  D(l00a);
  R(l00a);
  E;

  B(l01, Over 32 Kb of integers, 0)W(l01a);
   {
    c4_Storage s1("l01a", 1);
    s1.SetStructure("a[p1:I]");
    c4_View v1 = s1.View("a");
    c4_IntProp p1("p1");
    v1.SetSize(9000);

    for (int i = 0; i < v1.GetSize(); ++i) {
      p1(v1[i]) = 1000000L + i;

      A(p1(v1[i]) - 1000000L == i);
    }

    for (int j = 0; j < v1.GetSize(); ++j) {
      A(p1(v1[j]) - 1000000L == j);
    }

    s1.Commit();

    for (int k = 0; k < v1.GetSize(); ++k) {
      A(p1(v1[k]) - 1000000L == k);
    }

  }
  D(l01a);
  R(l01a);
  E;

  B(l02, Over 64 Kb of strings, 0)W(l02a);
   {
    static char *texts[3] =  {
      "Alice in Wonderland", "The wizard of Oz", "I'm singin' in the rain"
    };

    c4_Storage s1("l02a", 1);
    s1.SetStructure("a[p1:S]");
    c4_View v1 = s1.View("a");
    c4_StringProp p1("p1");
    c4_Row r1;

    for (int i = 0; i < 3500; ++i) {
      p1(r1) = texts[i % 3];
      v1.Add(r1);

      A(p1(v1[i]) == (c4_String)texts[i % 3]);
    }

    for (int j = 0; j < v1.GetSize(); ++j) {
      A(p1(v1[j]) == (c4_String)texts[j % 3]);
    }

    s1.Commit();

    for (int k = 0; k < v1.GetSize(); ++k) {
      A(p1(v1[k]) == (c4_String)texts[k % 3]);
    }

  }
  D(l02a);
  R(l02a);
  E;

  B(l03, Force sections in storage, 0)W(l03a);
  W(l03b);
   {
    c4_ViewProp p1("p1");
    c4_IntProp p2("p2");

     {
      c4_Storage s1("l03a", 1);
      s1.SetStructure("a[p1[p2:I]]");
      c4_View v1 = s1.View("a");

      c4_View v2;
      v2.SetSize(1);

      for (int i = 0; i < 500; ++i) {
        p2(v2[0]) = 9000+i;
        v1.Add(p1[v2]);
      }

      s1.Commit();
    }
     {
      c4_Storage s1("l03a", 0);
      c4_View v1 = s1.View("a");

      for (int i = 0; i < 500; ++i) {
        c4_View v2 = p1(v1[i]);
        A(p2(v2[0]) == 9000+i);
      }

      c4_FileStream fs1(fopen("l03b", "wb"), true);
      s1.SaveTo(fs1);
    }
     {
      c4_Storage s1;

      c4_FileStream fs1(fopen("l03b", "rb"), true);
      s1.LoadFrom(fs1);

      c4_View v1 = s1.View("a");

      for (int i = 0; i < 500; ++i) {
        c4_View v2 = p1(v1[i]);
        A(p2(v2[0]) == 9000+i);
      }
    }
  }
  D(l03a);
  D(l03b);
  R(l03a);
  R(l03b);
  E;

  B(l04, Modify sections in storage, 0)W(l04a);
   {
    c4_ViewProp p1("p1");
    c4_IntProp p2("p2");

     {
      c4_Storage s1("l04a", 1);
      s1.SetStructure("a[p1[p2:I]]");
      c4_View v1 = s1.View("a");

      c4_View v2;
      v2.SetSize(1);

      for (int i = 0; i < 500; ++i) {
        p2(v2[0]) = 9000+i;
        v1.Add(p1[v2]);
      }

      s1.Commit();
    }
     {
      c4_Storage s1("l04a", 1);
      c4_View v1 = s1.View("a");
      c4_View v2 = p1(v1[0]);

      p2(v2[0]) = 1;
      // this corrupted file in 1.5: free space was bad after load
      s1.Commit();
    }
     {
      c4_Storage s1("l04a", 0);
    }
  }
  D(l04a);
  R(l04a);
  E;

  B(l05, Delete from 32 Kb of strings, 0)W(l05a);
   {
    static char *texts[3] =  {
      "Alice in Wonderland", "The wizard of Oz", "I'm singin' in the rain"
    };

    c4_Storage s1("l05a", 1);
    s1.SetStructure("a[p1:I,p2:S,p3:S]");
    c4_View v1 = s1.View("a");
    c4_IntProp p1("p1");
    c4_StringProp p2("p2"), p3("p3");
    c4_Row r1;

    for (int i = 0; i < 1750; ++i) {
      p1(r1) = i;
      p2(r1) = texts[i % 3];
      p3(r1) = texts[i % 3];
      v1.Add(r1);

      A(p2(v1[i]) == (c4_String)texts[i % 3]);
    }

    for (int j = 0; j < v1.GetSize(); ++j) {
      A(p1(v1[j]) == j);
      A(p2(v1[j]) == (c4_String)texts[j % 3]);
      A(p3(v1[j]) == (c4_String)texts[j % 3]);
    }

    s1.Commit();

    while (v1.GetSize() > 1)
    // randomly remove entries
      v1.RemoveAt((unsigned short)(211 *v1.GetSize()) % v1.GetSize());

    s1.Commit();

  }
  D(l05a);
  R(l05a);
  E;

  B(l06, Bit field manipulations, 0)W(l06a);
   {
    c4_IntProp p1("p1");
    c4_View v2;

     {
      c4_Storage s1("l06a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      c4_Row r1;

      for (int i = 2; i <= 256; i <<= 1) {
        for (int j = 0; j < 18; ++j) {
          p1(r1) = j &(i - 1);

          v1.InsertAt(j, r1, j + 1);
          v2.InsertAt(j, r1, j + 1);
        }

        s1.Commit();
      }
    }
     {
      c4_Storage s1("l06a", 0);
      c4_View v1 = s1.View("a");

      int n = v2.GetSize();
      A(n == v1.GetSize());

      for (int i = 0; i < n; ++i) {
        long v = p1(v2[i]);
        A(p1(v1[i]) == v);
      }
    }

  }
  D(l06a);
  R(l06a);
  E;

  B(l07, Huge description, 0)W(l07a);
   {
    c4_String desc;

    for (int i = 1; i < 150; ++i) {
      char buf[50];
      // 1999-07-25: longer size to force over 4 Kb of description
      sprintf(buf, ",a123456789a123456789a123456789p%d:I", i);

      desc += buf;
    }

    desc = "a[" + desc.Mid(1) + "]";

    c4_Storage s1("l07a", 1);
    s1.SetStructure(desc);
    c4_View v1 = s1.View("a");
    c4_IntProp p123("p123");
    v1.Add(p123[123]);
    s1.Commit();

  }
  D(l07a);
  R(l07a);
  E;
}
