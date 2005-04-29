/* { dg-options "-faltivec -fdump-tree-ivu" } */

vector int vi = (vector int) (1,2,3,4);
int gi = 0;

/* Does not use vector operations.  */
int one ()
{
  return 1;
}

/* Uses vector operations.  */
int two ()
{
  vector int v2 = (vector int) (5,6,7,8);
  return one ();
}

/* Uses vector operations.  */
void three (vector int v1)
{
  vi = v1;
}

/* Uses vector operations.  */
void four (vector int v1)
{
  int gi;
  gi = 1;
}

/* Uses vector operations.  */
void five ()
{
  four ((vector int) (1,2,1,2));
}

/* Uses vector operations.  */
void six ()
{
  four (vi);
}

/* { dg-final { scan-tree-dump-times "one does not use vector operations." 1 "ivu" } } */
/* { dg-final { scan-tree-dump-times "two uses vector operations." 1 "ivu" } } */
/* { dg-final { scan-tree-dump-times "three uses vector operations." 1 "ivu" } } */
/* { dg-final { scan-tree-dump-times "four uses vector operations." 1 "ivu" } } */
/* { dg-final { scan-tree-dump-times "five uses vector operations." 1 "ivu" } } */
/* { dg-final { scan-tree-dump-times "six uses vector operations." 1 "ivu" } } */
