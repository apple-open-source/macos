/* APPLE LOCAL file AltiVec */
/* { dg-do run { target powerpc-apple-darwin* } } */
/* { dg-options "-faltivec" } */

/* Test for correct handling of AltiVec initializers (which
   look like comma expressions).  */

extern void abort(void);
#define CHECK_IF(expr) if(!(expr)) abort()

typedef vector signed int intvec;
struct vfoo { int x; vector signed int v; int y; };

struct vfoo vx_g = { (short)(10, 15, 20), (intvec)(11, 12, (9, 13), 14), (int)(15, 16) };

int main(void) {
  signed int *vec = reinterpret_cast<signed int *>(&vx_g.v);
  CHECK_IF(vx_g.x == 20 && vx_g.y == 16);
  CHECK_IF(vec[0] == 11 && vec[1] == 12 && vec[2] == 13 && vec[3] == 14);
  return 0;
}

