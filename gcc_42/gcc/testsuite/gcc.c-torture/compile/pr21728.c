/* APPLE LOCAL testsuite nested functions */
/* { dg-options "-fnested-functions" } */
int main (void)
{
  __label__ l1;
  void __attribute__((used)) q(void)
  {
    goto l1;
  }

  l1:;
}
