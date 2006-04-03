/* APPLE LOCAL  nested functions */
/* { dg-xfail-if "" { "*-*-darwin*" } { "*" } { "" } } */
void baz(int i);

void foo(int i, int A[i+1])
{
    int j=A[i];
    void bar() { baz(A[i]); }
}
