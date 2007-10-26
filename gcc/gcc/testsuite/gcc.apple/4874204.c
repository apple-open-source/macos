/* APPLE LOCAL file 4874204 */
/* { dg-do run } */
/* { dg-options "-O1" } */
typedef struct node
{
  struct node *p;
  unsigned     a : 12;
  unsigned     b : 8;
  unsigned     c : 9;
  unsigned     d : 1;
  unsigned     e : 1;
  unsigned     f : 1;
} T;

extern void abort (void);

void foo (T * tn, int n)
{
  tn->a = 0xfff;
  tn->b = 0xff;
  tn->c = n;
  tn->d = 1;
  tn->e = 1;
  tn->f = 1;
}

void bar (T * tn)
{
  int i;

  for (i = 0; i < tn->c; ++i)
    abort();
}

int main()
{
  T tn;
  foo (&tn, 0);
  bar (&tn);
  return 0;
}
