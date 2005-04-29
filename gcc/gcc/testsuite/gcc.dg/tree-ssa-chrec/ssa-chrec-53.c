/* APPLE LOCAL file lno */
/* { dg-do compile } */
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details  " } */

#define N 16

void fbar (float *);
void ibar (int *);
void sbar (short *);

/* Should be vectorized */

foo (int n)
{
  float a[N+1];
  float b[N];
  float c[N];
  float d[N];
  int ia[N];
  int ib[N];
  int ic[N];
  double da[N];
  double db[N];
  short sa[N];
  short sb[N];
  short sc[N];
  int i,j;
  int diff = 0;
  char cb[N];
  char cc[N];
  char image[N][N];
  char block[N][N];

  /* Not vetorizable yet (unknown loop bound).  */
  for (i = 0; i < n; i++){
    a[i] = b[i];
  }
  fbar (a);

  /* Vectorizable.  */
  for (i = 0; i < N; i++){
    a[i] = b[i];
  }
  fbar (a);

  /* Not Vectorizable (mode not supported).  */
  for (i = 0; i < N; i++){
    da[i] = db[i];
  }
  fbar (a);

  /* Not vetorizable yet (constant assignment).  */
  for (i = 0; i < N; i++){
    a[i] = 5;
  }
  fbar (a);

  /* Vectorizable.  */
  for (i = 0; i < N; i++){
    a[i] = b[i] + c[i] + d[i];
  }
  fbar (a);

  /* Vectorizable.  */
  for (i = 0; i < N; i++){
    a[i] = b[i] * c[i];
  }
  fbar (a);

  /* Vectorizable.  */
  for (i = 0; i < N/2; i++){
    a[i] = b[i+N/2] * c[i+N/2] - b[i] * c[i];
    d[i] = b[i] * c[i+N/2] + b[i+N/2] * c[i];
  }
  fbar (a);

  /* Not vetorizable yet (too conservative dependence test).  */
  for (i = 0; i < N/2; i++){
    a[i] = b[i+N/2] * c[i+N/2] - b[i] * c[i];
    a[i+N/2] = b[i] * c[i+N/2] + b[i+N/2] * c[i];
  }
  fbar (a);

  /* Not vetorizable yet (access pattern).  */
  for (i = 0; i < N/2; i++){
    a[i] = b[2*i+1] * c[2*i+1] - b[2*i] * c[2*i];
    d[i] = b[2*i] * c[2*i+1] + b[2*i+1] * c[2*i];
  }
  fbar (a);

  /* Not vetorizable yet (too conservative dependence test; access pattern).  */
  for (i = 0; i < N/2; i++){
    a[2*i] = b[2*i+1] * c[2*i+1] - b[2*i] * c[2*i];
    a[2*i+1] = b[2*i] * c[2*i+1] + b[2*i+1] * c[2*i];
  }
  fbar (a);

  /* Not vetorizable yet (no support for integer mult).  */
  for (i = 0; i < N; i++){
    ia[i] = ib[i] * ic[i];
  }
  ibar (ia);

  /* Vectorizable.  */
  for (i = 0; i < N; i++){
    a[i] = b[i] + c[i];
    d[i] = b[i] + c[i];
    ia[i] = ib[i] + ic[i];
  }
  ibar (ia);
  fbar (a);
  fbar (d);

  /* Not vectorizable yet (two types with different nunits in vector).  */
  for (i = 0; i < N; i++){
    ia[i] = ib[i] + ic[i];
    sa[i] = sb[i] + sc[i];
  }
  ibar (ia);
  sbar (sa);

  /* Not vetorizable yet (too conservative dependence test).  */
  for (i = 0; i < N; i++){
    a[i] = b[i] + c[i];
    a[i+1] = b[i] + c[i];
  }
  fbar (a);
}

