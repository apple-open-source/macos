/* APPLE LOCAL begin begin radar 4257049 */
/* { dg-do assemble { target powerpc*-*-* } } */
/* { dg-options "-fasm-blocks" } */

namespace foobar {
typedef unsigned long type;
}

namespace F {

typedef unsigned long testAsmFuncType;

extern testAsmFuncType testAsmFunction();

asm testAsmFuncType

testAsmFunction()

{
        nofralloc
        li      r3, 1
        blr
}

asm foobar::type another()
{
 nop
}
}

namespace {
  class CI {
    typedef int foo;
    static asm foo bar () { nop }
  };
}

template <class T> class C {
    typedef typename T::bar foo;
public:
    static asm foo bar () { nop }
};

class B {
public:
    typedef int bar;
};

C<B> e;

int main()
{
        return e.bar();
}
/* APPLE LOCAL end radar 4257049 */
