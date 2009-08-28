// trseize.cpp -- Regression test program, resizing tests
// $Id: tresize.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "regress.h"

#include <stdlib.h>   // strtol
#include <string.h>   // memory functions

class CResizer: public c4_Storage {
  public:
    CResizer(const char *file);
    ~CResizer();

    void Verify();

    int Ins(int, int);
    int Del(int, int);

  private:
    enum {
        kMaxData = 15000
    };
    char *_refData;
    int _refSize;

    c4_View _attached;
    c4_View _unattached;
    c4_IntProp _prop;

    char _seed;

    CResizer(const CResizer &); // not implemented
    void operator = (const CResizer &); // not implemented
};

CResizer::CResizer(const char *file): c4_Storage(file, 1), _refSize(0), _prop(
  "p1"), _seed(0) {
  SetStructure("a[p1:I]");

  _refData = new char[kMaxData];

  _attached = View("a");

  Verify();
}

CResizer::~CResizer() {
  Verify();

  Commit();

  Verify();

  delete [] _refData;
}

void CResizer::Verify() {
  int i;

  A(_refSize == _unattached.GetSize());
  A(_refSize == _attached.GetSize());

  for (i = 0; i < _refSize; ++i) {
    A(_refData[i] == _prop(_unattached[i]));
    A(_refData[i] == _prop(_attached[i]));
  }
}

int CResizer::Ins(int pos_, int cnt_) {
  A(pos_ <= _refSize);
  A(_refSize + cnt_ < kMaxData);

  memmove(_refData + pos_ + cnt_, _refData + pos_, _refSize - pos_);
  _refSize += cnt_;

  c4_Row row;
  _unattached.InsertAt(pos_, row, cnt_);
  _attached.InsertAt(pos_, row, cnt_);

  for (int i = pos_; i < pos_ + cnt_; ++i) {
    _refData[i] = ++_seed;
    _prop(_unattached[i]) = _seed;
    _prop(_attached[i]) = _seed;

    if (_seed >= 123)
      _seed = 0;
  }

  Verify();

  return _refSize;
}

int CResizer::Del(int pos_, int cnt_) {
  A(pos_ + cnt_ <= _refSize);

  _refSize -= cnt_;
  memmove(_refData + pos_, _refData + pos_ + cnt_, _refSize - pos_);

  _unattached.RemoveAt(pos_, cnt_);
  _attached.RemoveAt(pos_, cnt_);

  Verify();

  return _refSize;
}

void TestResize() {
  B(r00, Simple insert, 0)W(r00a);
   {
    CResizer r1("r00a");

    int n = r1.Ins(0, 250);
    A(n == 250);

  }
  D(r00a);
  R(r00a);
  E;

  B(r01, Simple removes, 0)W(r01a);
   {
    CResizer r1("r01a");
    int n;

    n = r1.Ins(0, 500);
    A(n == 500);

    n = r1.Del(0, 50);
    A(n == 450);
    n = r1.Del(350, 100);
    A(n == 350);
    n = r1.Del(25, 150);
    A(n == 200);
    n = r1.Del(0, 200);
    A(n == 0);

    n = r1.Ins(0, 15);
    A(n == 15);

  }
  D(r01a);
  R(r01a);
  E;

  B(r02, Large inserts and removes, 0)W(r02a);
   {
    int big = sizeof(int) == sizeof(short) ? 1000 : 4000;

    CResizer r1("r02a");
    int n;

    n = r1.Ins(0, 2000);
    A(n == 2000);
    n = r1.Ins(0, 3000);
    A(n == 5000);
    n = r1.Ins(5000, 1000+big);
    A(n == 6000+big);
    n = r1.Ins(100, 10);
    A(n == 6010+big);
    n = r1.Ins(4000, 100);
    A(n == 6110+big);
    n = r1.Ins(0, 1001);
    A(n == 7111+big);

    n = r1.Del(7111, big);
    A(n == 7111);
    n = r1.Del(0, 4111);
    A(n == 3000);
    n = r1.Del(10, 10);
    A(n == 2990);
    n = r1.Del(10, 10);
    A(n == 2980);
    n = r1.Del(5, 10);
    A(n == 2970);
    n = r1.Del(0, 990);
    A(n == 1980);
    n = r1.Del(3, 1975);
    A(n == 5);

  }
  D(r02a);
  R(r02a);
  E;

  B(r03, Binary property insertions, 0)W(r03a);
   {
    c4_BytesProp p1("p1");
    c4_Storage s1("r03a", 1);
    s1.SetStructure("a[p1:B]");
    c4_View v1 = s1.View("a");

    char buf[1024];

    memset(buf, 0x11, sizeof buf);
    v1.Add(p1[c4_Bytes(buf, sizeof buf)]);

    memset(buf, 0x22, sizeof buf);
    v1.Add(p1[c4_Bytes(buf, sizeof buf / 2)]);

    s1.Commit();

    memset(buf, 0x33, sizeof buf);
    p1(v1[1]) = c4_Bytes(buf, sizeof buf); // fix c4_Column::CopyData

    memset(buf, 0x44, sizeof buf);
    v1.Add(p1[c4_Bytes(buf, sizeof buf / 3)]);

    s1.Commit();

    memset(buf, 0x55, sizeof buf);
    v1.InsertAt(1, p1[c4_Bytes(buf, sizeof buf)]);

    memset(buf, 0x66, sizeof buf);
    v1.InsertAt(1, p1[c4_Bytes(buf, sizeof buf / 4)]);

    s1.Commit();

  }
  D(r03a);
  R(r03a);
  E;

  B(r04, Scripted string property tests, 0)W(r04a);
   {
    c4_StringProp p1("p1");
    c4_Storage s1("r04a", 1);
    s1.SetStructure("a[p1:S]");

    // This code implements a tiny language to specify tests in:
    //
    //  "<X>,<Y>A"  add X partial buffers of size Y
    //  "<X>a"    add X full buffers at end
    //  "<X>,<Y>C"  change entry X to a partial buffer of size Y
    //  "<X>c"    change entry at position X to a full buffer
    //  "<X>,<Y>I"  insert partial buffer of size Y at position X
    //  "<X>i"    insert a full buffer at position X
    //  "<X>,<Y>R"  remove Y entries at position X
    //  "<X>r"    remove one entry at position X
    //
    //  ">"     commit changes
    //  "<"     rollback changes
    //
    //  " "     ignore spaces
    //  "<X>,"    for additional args
    //  "<X>="    verify number of rows is X

    const char *scripts[] =  {
      //   A  B  C  D    E    F   G    H    I J
      "5a 5a 5a 1r   5r   10r   6r     2r   > 10=", 
        "5a 5a 5a 1,200C 5,200C 10,200C 6,200C 2,200C > 15=", 
        "5a 5a 5a 1,300C 5,300C 10,300C 6,300C 2,300C > 15=", 

      //   A   B   C   D     E     F      G     H     I J
      "50a 50a 50a 10r   50r   100r   60r   20r   > 145=", 
        "50a 50a 50a 10,200C 50,200C 100,200C 60,200C 20,200C > 150=", 
        "50a 50a 50a 10,300C 50,300C 100,300C 60,300C 20,300C > 150=", 

      //   A     B   C     D   E   F  G H I J
      "50,0A 50,0A 50,0A 10c 50c 100c 60c 20c > 150=",  // asserts in 1.7b1

      //   A    B   C  D E
      "3,3A 1,10C 1,1C > 3=",  // asserts in 1.7 - June 6 build

      "", 0
    };

    for (int i = 0; scripts[i]; ++i) {
      c4_View v1 = s1.View("a");
      v1.RemoveAll();
      s1.Commit();
      A(v1.GetSize() == 0); // start with a clean slate each time

      const char *p = scripts[i];

      char fill = '@';
      int save = 0;
      c4_Row row;

      while (*p) {
        // default is a string of 255 chars (with additional null byte)
        p1(row) = c4_String(++fill, 255);

        int arg = (int)strtol(p, (char **) &p, 10); // loses const

        switch (*p++) {
          case 'A':
            p1(row) = c4_String(fill, arg);
            arg = save;
          case 'a':
            while (--arg >= 0)
              v1.Add(row);
            break;
          case 'C':
            p1(row) = c4_String(fill, arg);
            arg = save;
          case 'c':
            v1.SetAt(arg, row);
            break;
          case 'I':
            p1(row) = c4_String(fill, arg);
            arg = save;
          case 'i':
            v1.InsertAt(arg, row);
            break;
          case 'R':
            v1.RemoveAt(save, arg);
            break;
          case 'r':
            v1.RemoveAt(arg);
            break;
          case '>':
            s1.Commit();
            break;
          case '<':
            s1.Rollback();
            v1 = s1.View("a");
            break;
          case ' ':
            break;
          case ',':
            save = arg;
            break;
          case '=':
            A(v1.GetSize() == arg);
            break;
        }
      }
    }

    s1.Commit();

  }
  D(r04a);
  R(r04a);
  E;
}
