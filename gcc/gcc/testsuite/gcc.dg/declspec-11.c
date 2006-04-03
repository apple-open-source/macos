/* Test declaration specifiers.  Test various checks on storage class
   and function specifiers that depend on information about the
   declaration, not just the specifiers.  Test with -pedantic-errors.  */
/* Origin: Joseph Myers <jsm@polyomino.org.uk> */
/* { dg-do compile } */
/* { dg-options "-pedantic-errors" } */

auto void f0 (void) {} /* { dg-error "error: function definition declared 'auto'" } */
register void f1 (void) {} /* { dg-error "error: function definition declared 'register'" } */
typedef void f2 (void) {} /* { dg-error "error: function definition declared 'typedef'" } */

void f3 (auto int); /* { dg-error "error: storage class specified for parameter 'type name'" } */
void f4 (extern int); /* { dg-error "error: storage class specified for parameter 'type name'" } */
void f5 (register int);
void f6 (static int); /* { dg-error "error: storage class specified for parameter 'type name'" } */
void f7 (typedef int); /* { dg-error "error: storage class specified for parameter 'type name'" } */

auto int x; /* { dg-error "error: file-scope declaration of 'x' specifies 'auto'" } */
register int y; /* { dg-error "error: file-scope declaration of 'y' specifies 'register'" } */

/* APPLE LOCAL begin testsuite nested functions */
void h (void) { extern void x (void) {} } /* { dg-error "error: nested function 'x' declared 'extern'|nested functions are not supported on MacOSX" } */
/* { dg-error "error: ISO C forbids nested functions" "nested" { target *-*-* } 22 } */
/* APPLE LOCAL end testsuite nested functions */
void
g (void)
{
  void a; /* { dg-error "error: variable or field 'a' declared void" } */
  const void b; /* { dg-error "error: variable or field 'b' declared void" } */
  static void c; /* { dg-error "error: variable or field 'c' declared void" } */
}

void p;
const void p1;
extern void q;
extern const void q1;
static void r; /* { dg-error "error: variable or field 'r' declared void" } */
static const void r1; /* { dg-error "error: variable or field 'r1' declared void" } */

/* APPLE LOCAL begin testsuite nested functions */
register void f8 (void); /* { dg-error "error: invalid storage class for function 'f8'|nested functions are not supported on MacOSX" } */
/* { dg-error "error: file-scope declaration of 'f8' specifies 'register'" "register function" { target *-*-* } 41 } */

void i (void) { auto void y (void) {} } /* { dg-error "error: ISO C forbids nested functions|nested functions are not supported on MacOSX" } */

/* { dg-error "error: function definition declared 'auto'" "nested" { target *-*-* } 44 } */
/* APPLE LOCAL end testsuite nested functions */
inline int main (void) { return 0; } /* { dg-error "error: cannot inline function 'main'" } */
