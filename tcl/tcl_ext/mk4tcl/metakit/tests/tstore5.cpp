// tstore5.cpp -- Regression test program, storage tests, part 5
// $Id: tstore5.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestStores5() {
  B(s40, LoadFrom after commit, 0)W(s40a);
   {
    c4_IntProp p1("p1");

     {
      // create datafile by streaming out
      c4_Storage s1;
      s1.SetStructure("a[p1:I]");

      c4_View v1 = s1.View("a");
      v1.Add(p1[123]);
      A(p1(v1[0]) == 123);
      A(v1.GetSize() == 1);

      c4_FileStream fs1(fopen("s40a", "wb"), true);
      s1.SaveTo(fs1);
    }
     {
      // it should load just fine
      c4_Storage s2;
      c4_FileStream fs1(fopen("s40a", "rb"), true);
      bool ok = s2.LoadFrom(fs1);
      A(ok);

      c4_View v1 = s2.View("a");
      A(p1(v1[0]) == 123);
      A(v1.GetSize() == 1);
    }
     {
      // open the datafile and commit a change
      c4_Storage s3("s40a", true);

      c4_View v1 = s3.View("a");
      A(p1(v1[0]) == 123);
      A(v1.GetSize() == 1);
      p1(v1[0]) = 456;
      s3.Commit();
      A(p1(v1[0]) == 456);
      A(v1.GetSize() == 1);
    }
     {
      // it should load fine and show the last changes
      c4_Storage s4;
      c4_FileStream fs1(fopen("s40a", "rb"), true);
      bool ok = s4.LoadFrom(fs1);
      A(ok);

      c4_View v1 = s4.View("a");
      A(p1(v1[0]) == 456);
      A(v1.GetSize() == 1);
    }
     {
      // it should open just fine in the normal way as well
      c4_Storage s5("s40a", false);
      c4_View v1 = s5.View("a");
      A(p1(v1[0]) == 456);
      A(v1.GetSize() == 1);
    }
  }
  D(s40a);
  R(s40a);
  E;

  // 2002-03-13: failure on Win32, Modify calls base class GetNthMemoCol
  B(s41, Partial modify blocked, 0)W(s41a);
   {
    c4_BytesProp p1("p1");
    c4_Storage s1("s41a", true);
    c4_View v1 = s1.GetAs("a[_B[p1:B]]");

    // custom viewers did not support partial access in 2.4.3
    c4_View v2 = v1.Blocked();
    s1.Commit();

    v2.SetSize(1);

    c4_BytesRef m = p1(v2[0]);
    m.Modify(c4_Bytes("abcdefgh", 8), 0);

    s1.Commit();

  }
  D(s41a);
  R(s41a);
  E;

  B(s42, Get descriptions, 0) {
    c4_Storage s1;
    s1.SetStructure("a[p1:I],b[p2:S]");

    c4_String x1 = s1.Description();
    A(x1 == "a[p1:I],b[p2:S]");

    c4_String x2 = s1.Description("b");
    A(x2 == "p2:S");

    const char *cp = s1.Description("c");
    A(cp == 0);
  }
  E;

  // 2002-04-24: VPI subview ints clobbered
  B(s43, View reuse after sub-byte ints, 0)W(s43a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("s43a", true);
    c4_View v1 = s1.GetAs("a[p1:I]");

    v1.Add(p1[0]);
    v1.Add(p1[1]);
    s1.Commit();

    v1.SetSize(1); // 1 is an even trickier bug than 0
    s1.Commit();

    // adding the following two lines works around the 2.4.4 bug
    //s1.Rollback();
    //v1 = s1.GetAs("a[p1:I]");

    v1.Add(p1[12345]);
    s1.Commit();

    //int n = p1 (v1[1]);
    A(p1(v1[1]) == 12345);

  }
  D(s43a);
  R(s43a);
  E;

  B(s44, Bad memo free space, 0)W(s44a);
   {
    c4_IntProp p1("p1");
    c4_BytesProp p2("p2");
    c4_Storage s1("s44a", true);
    c4_View v1 = s1.GetAs("a[p1:I,p2:B]");

    c4_Bytes data;
    t4_byte *p = data.SetBuffer(12345);
    for (int i = 0; i < data.Size(); ++i)
      p[i] = (t4_byte)i;

    v1.Add(p2[data]);
    s1.Commit();

    p1(v1[0]) = 1;
    s1.Commit();

    p1(v1[0]) = 0;
    s1.Commit();

    c4_Bytes temp = p2(v1[0]);
    A(temp == data); // this failed in 2.4.5

  }
  D(s44a);
  R(s44a);
  E;

  B(s45, Bad subview memo free space, 0)W(s45a);
   {
    c4_IntProp p1("p1");
    c4_ViewProp p2("p2");
    c4_BytesProp p3("p3");
    c4_Storage s1("s45a", true);
    c4_View v1 = s1.GetAs("a[p1:I,p2[p3:B]]");

    c4_Bytes data;
    t4_byte *p = data.SetBuffer(12345);
    for (int i = 0; i < data.Size(); ++i)
      p[i] = (t4_byte)i;

    v1.SetSize(1);
    c4_View v2 = p2(v1[0]);
    v2.Add(p3[data]);
    s1.Commit();

    p1(v1[0]) = 1;
    s1.Commit();

    p1(v1[0]) = 0;
    s1.Commit();

    c4_View v3 = p2(v1[0]);
    c4_Bytes temp = p3(v3[0]);
    A(temp == data); // this failed in 2.4.5

  }
  D(s45a);
  R(s45a);
  E;

  B(s46, LoadFrom after commit, 0)W(s46a);
   {
    c4_IntProp p1("p1");

     {
      c4_Storage s1("s46a", true);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");

      v1.Add(p1[11]);
      v1.Add(p1[22]);
      v1.Add(p1[33]);
      v1.Add(p1[44]);
      v1.Add(p1[55]);
      v1.Add(p1[66]);
      v1.Add(p1[77]);
      v1.Add(p1[88]);
      v1.Add(p1[99]);

      s1.Commit();
    }
     {
      c4_Storage s2("s46a", true);
      c4_View v2 = s2.View("a");

      v2.Add(p1[1000]); // force 1->2 byte ints
      v2.InsertAt(7, c4_Row());
      v2.InsertAt(4, c4_Row());

      //for (int i = 6; i <= 9; ++i) printf("%d\n", (int) p1 (v2[i]));

      A(p1(v2[6]) == 66);
      A(p1(v2[8]) == 0);
      A(p1(v2[9]) == 88);
      A(p1(v2[7]) == 77); // this failed in 2.4.6

      s2.Commit();
    }
  }
  D(s46a);
  R(s46a);
  E;

  // 2004-01-16 bad property type crashes MK 2.4.9.2 and before
  // this hits an assertion in debug mode, so then it has to be disabled
  B(s47, Defining bad property type, 0) {
    c4_IntProp p1("p2");

    c4_Storage s1;
#if defined(NDEBUG)
    c4_View v1 = s1.GetAs("v1[p1:A]");
#else 
    // assertions are enabled, turn this into a dummy test instead
    c4_View v1 = s1.GetAs("v1[p1:I]");
#endif 
    v1.Add(p1[123]);

    A(v1.GetSize() == 1);
    A(p1(v1[0]) == 123);
  }
  E;

  // 2004-01-18 file damaging bug, when resizing a comitted subview
  // to empty, committing, and then resizing back to containing data.
  // Fortunately this usage pattern never happened in blocked views!
  B(s48, Resize subview to zero and back, 0)W(s48a);
  W(s48b);
   {
     {
      c4_Storage s1("s48a", true);
      c4_View v1 = s1.GetAs("v1[v2[p1:I]]");
      v1.SetSize(1);
      s1.Commit();
    }
     {
      c4_Storage s1("s48a", true);
      c4_View v1 = s1.View("v1");
      v1.SetSize(0);
      s1.Commit();
      // the problem is that the in-memory copy has forgotten that it
      // has nothing left on disk, and a comparison is done later on to
      // avoid saving unmodified data - the bad decision is that data has
      // not changed, but actually it has and must be reallocated!
      // (fixes are in c4_FormatV::Insert and c4_FormatV::Remove)
      v1.SetSize(1);
      s1.Commit();
      // at this point, the 2.4.9.2 file is corrupt!
      c4_FileStream fs1(fopen("s48b", "wb"), true);
      s1.SaveTo(fs1);
    }
     {
      // using this damaged datafile will then crash
      c4_Storage s1("s48a", false);
      c4_View v1 = s1.View("v1");
      v1.SetSize(2);
    }
  }
  D(s48a);
  D(s48b);
  R(s48a);
  R(s48b);
  E;

  // 2004-01-20 better handling of bad input: ignore repeated props
  B(s49, Specify conflicting properties, 0)W(s49a);
   {
    c4_Storage s1("s49a", true);
    c4_View v1 = s1.GetAs("v1[p1:I,p1:S]");
    c4_View v2 = s1.GetAs("v2[p1:I,P1:S]");
    c4_View v3 = s1.GetAs("v3[v3[^]]");
    c4_String x1 = s1.Description();
    A(x1 == "v1[p1:I],v2[p1:I],v3[v3[^]]");
    s1.Commit();
  }
  D(s49a);
  R(s49a);
  E;

  B(s50, Free space usage, 0)W(s50a);
   {
    t4_i32 c, b;
    c4_IntProp p1("p1");

    c4_Storage s1("s50a", true);
    c4_View v1 = s1.GetAs("a[p1:I]");

    v1.Add(p1[12345]);

    s1.Commit();
    c = s1.FreeSpace(&b);
    A(c == 0);
    A(b == 0);

    v1.Add(p1[2345]);

    s1.Commit();
    c = s1.FreeSpace(&b);
    A(c == 1);
    A(b == 18);
    s1.Commit();
    c = s1.FreeSpace(&b);
    A(c == 1);
    A(b == 6);

    v1.Add(p1[345]);

    s1.Commit();
    c = s1.FreeSpace(&b);
    A(c == 2);
    A(b == 56);
    s1.Commit();
    c = s1.FreeSpace(&b);
    A(c == 1);
    A(b == 44);
    //fprintf(stderr, "c %d b %d\n", c, b);
  }
  D(s50a);
  R(s50a);
  E;
}
