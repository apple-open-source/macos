// tstore4.cpp -- Regression test program, storage tests, part 4
// $Id: tstore4.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

void TestStores4() {
  B(s30, Memo storage, 0)W(s30a);
   {
    c4_Bytes hi("hi", 2);
    c4_Bytes gday("gday", 4);
    c4_Bytes hello("hello", 5);

    c4_MemoProp p1("p1");
    c4_Storage s1("s30a", 1);
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
  D(s30a);
  R(s30a);
  E;

  // this failed in the unbuffered 1.8.5a interim release in Mk4tcl 1.0.5
  B(s31, Check sort buffer use, 0)W(s31a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("s31a", 1);
    s1.SetStructure("a[p1:I]");
    c4_View v1 = s1.View("a");
    v1.Add(p1[3]);
    v1.Add(p1[1]);
    v1.Add(p1[2]);
    s1.Commit();

    c4_View v2 = v1.SortOn(p1);
    A(v2.GetSize() == 3);
    A(p1(v2[0]) == 1);
    A(p1(v2[1]) == 2);
    A(p1(v2[2]) == 3);

  }
  D(s31a);
  R(s31a);
  E;

  // this failed in 1.8.6, fixed 19990828
  B(s32, Set memo empty or same size, 0)W(s32a);
   {
    c4_Bytes empty;
    c4_Bytes full("full", 4);
    c4_Bytes more("more", 4);

    c4_MemoProp p1("p1");
    c4_Storage s1("s32a", 1);
    s1.SetStructure("a[p1:B]");
    c4_View v1 = s1.View("a");

    v1.Add(p1[full]);
    A(p1(v1[0]) == full);
    s1.Commit();
    A(p1(v1[0]) == full);

    p1(v1[0]) = empty;
    A(p1(v1[0]) == empty);
    s1.Commit();
    A(p1(v1[0]) == empty);

    p1(v1[0]) = more;
    A(p1(v1[0]) == more);
    s1.Commit();
    A(p1(v1[0]) == more);

    p1(v1[0]) = full;
    A(p1(v1[0]) == full);
    s1.Commit();
    A(p1(v1[0]) == full);

  }
  D(s32a);
  R(s32a);
  E;

  // this failed in 1.8.6, fixed 19990828
  B(s33, Serialize memo fields, 0)W(s33a);
  W(s33b);
  W(s33c);
   {
    c4_Bytes hi("hi", 2);
    c4_Bytes gday("gday", 4);
    c4_Bytes hello("hello", 5);

    c4_MemoProp p1("p1");

    c4_Storage s1("s33a", 1);
    s1.SetStructure("a[p1:B]");
    c4_View v1 = s1.View("a");

    v1.Add(p1[hi]);
    v1.Add(p1[gday]);
    v1.Add(p1[hello]);
    A(p1(v1[0]) == hi);
    A(p1(v1[1]) == gday);
    A(p1(v1[2]) == hello);
    s1.Commit();
    A(p1(v1[0]) == hi);
    A(p1(v1[1]) == gday);
    A(p1(v1[2]) == hello);

     {
      c4_FileStream fs1(fopen("s33b", "wb"), true);
      s1.SaveTo(fs1);
    }

    c4_Storage s2("s33c", 1);

    c4_FileStream fs2(fopen("s33b", "rb"), true);
    s2.LoadFrom(fs2);

    c4_View v2 = s2.View("a");
    A(p1(v2[0]) == hi);
    A(p1(v2[1]) == gday);
    A(p1(v2[2]) == hello);
    s2.Commit();
    A(p1(v2[0]) == hi);
    A(p1(v2[1]) == gday);
    A(p1(v2[2]) == hello);
    s2.Commit();
    A(p1(v2[0]) == hi);
    A(p1(v2[1]) == gday);
    A(p1(v2[2]) == hello);

  }
  D(s33a);
  D(s33b);
  D(s33c);
  R(s33a);
  R(s33b);
  R(s33c);
  E;

  // check smarter commit and commit failure on r/o
  B(s34, Smart and failed commits, 0)W(s34a);
   {
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s34a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[111]);
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 111);
      bool f1 = s1.Commit();
      A(f1);
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 111);
      bool f2 = s1.Commit();
      A(f2); // succeeds, but should not write anything
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 111);
    }
     {
      c4_Storage s1("s34a", 0);
      c4_View v1 = s1.View("a");
      v1.Add(p1[222]);
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 111);
      A(p1(v1[1]) == 222);
      bool f1 = s1.Commit();
      A(!f1);
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 111);
      A(p1(v1[1]) == 222);
    }
  }
  D(s34a);
  R(s34a);
  E;

  B(s35, Datafile with preamble, 0)W(s35a);
   {
     {
      c4_FileStream fs1(fopen("s35a", "wb"), true);
      fs1.Write("abc", 3);
    }
    c4_IntProp p1("p1");
     {
      c4_Storage s1("s35a", 1);
      s1.SetStructure("a[p1:I]");
      c4_View v1 = s1.View("a");
      v1.Add(p1[111]);
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 111);
      bool f1 = s1.Commit();
      A(f1);
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 111);
      bool f2 = s1.Commit();
      A(f2); // succeeds, but should not write anything
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 111);
    }
     {
      c4_FileStream fs1(fopen("s35a", "rb"), true);
      char buffer[10];
      int n1 = fs1.Read(buffer, 3);
      A(n1 == 3);
      A(c4_String(buffer, 3) == "abc");
    }
     {
      c4_Storage s1("s35a", 0);
      c4_View v1 = s1.View("a");
      A(v1.GetSize() == 1);
      A(p1(v1[0]) == 111);
      v1.Add(p1[222]);
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 111);
      A(p1(v1[1]) == 222);
      bool f1 = s1.Commit();
      A(!f1);
      A(v1.GetSize() == 2);
      A(p1(v1[0]) == 111);
      A(p1(v1[1]) == 222);
    }
  }
  D(s35a);
  R(s35a);
  E;

  B(s36, Commit after load, 0)W(s36a);
  W(s36b);
   {
    c4_IntProp p1("p1");

    c4_Storage s1("s36a", 1);
    s1.SetStructure("a[p1:I]");
    c4_View v1 = s1.View("a");
    v1.Add(p1[111]);
    A(v1.GetSize() == 1);
    A(p1(v1[0]) == 111);

     {
      c4_FileStream fs1(fopen("s36b", "wb"), true);
      s1.SaveTo(fs1);
    }

    p1(v1[0]) = 222;
    v1.Add(p1[333]);
    bool f1 = s1.Commit();
    A(f1);
    A(v1.GetSize() == 2);
    A(p1(v1[0]) == 222);
    A(p1(v1[1]) == 333);

    c4_FileStream fs2(fopen("s36b", "rb"), true);
    s1.LoadFrom(fs2);
    //A(v1.GetSize() == 0); // should be detached, but it's still 2

    c4_View v2 = s1.View("a");
    A(v2.GetSize() == 1);
    A(p1(v2[0]) == 111);

    // this fails in 2.4.0, reported by James Lupo, August 2001
    bool f2 = s1.Commit();
    A(f2);
  }
  D(s36a);
  D(s36b);
  R(s36a);
  R(s36b);
  E;

  // fails in 2.4.1, reported Oct 31. 2001 by Steve Baxter
  B(s37, Change short partial fields, 0)W(s37a);
   {
    c4_BytesProp p1("p1");
    c4_Storage s1("s37a", true);
    c4_View v1 = s1.GetAs("v1[key:I,p1:B]");

    v1.Add(p1[c4_Bytes("12345", 6)]);
    A(v1.GetSize() == 1);
    s1.Commit();

    c4_Bytes buf = p1(v1[0]);
    A(buf.Size() == 6);
    A(buf == c4_Bytes("12345", 6));
    buf = p1(v1[0]).Access(1, 3);
    A(buf == c4_Bytes("234", 3));
    p1(v1[0]).Modify(c4_Bytes("ab", 2), 2, 0);
    s1.Commit();

    buf = p1(v1[0]);
    A(buf == c4_Bytes("12ab5", 6));
  }
  D(s37a);
  R(s37a);
  E;

  // Gross memory use (but no leaks), January 2002, Murat Berk
  B(s38, Lots of empty subviews, 0)W(s38a);
   {
    c4_BytesProp p1("p1");
     {
      c4_Storage s1("s38a", true);
      c4_View v = s1.GetAs("v[v1[p1:S]]");

      v.SetSize(100000);
      s1.Commit();
    }
     {
      c4_Storage s2("s38a", true);
      c4_View v2 = s2.View("v");
      // this should not materialize all the empty subviews
      v2.SetSize(v2.GetSize() + 1);
      // nor should this
      s2.Commit();
    }
     {
      c4_Storage s3("s38a", true);
      c4_View v3 = s3.View("v");
      v3.RemoveAt(1, v3.GetSize() - 2);
      A(v3.GetSize() == 2);
      s3.Commit();
    }
  }
  D(s38a);
  R(s38a);
  E;

  // Fix bug introduced on 7-2-2002, as reported by M. Berk
  B(s39, Do not detach empty top-level views, 0)W(s39a);
   {
    c4_IntProp p1("p1");
    c4_Storage s1("s39a", true);
    c4_View v1 = s1.GetAs("v1[p1:I]");
    s1.Commit();
    A(v1.GetSize() == 0);
    v1.Add(p1[123]);
    A(v1.GetSize() == 1);
    s1.Commit();
    c4_View v2 = s1.View("v1");
    A(v2.GetSize() == 1); // fails with 0 due to recent bug
  }
  D(s39a);
  R(s39a);
  E;
}
