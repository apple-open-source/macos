/* APPLE LOCAL file AltiVec */
/* { dg-do compile { target powerpc*-*-* } } */
/* { dg-options "-faltivec" } */

int main(void)
{
        bool b;  /* { dg-error ".bool. undeclared" } */
        /* { dg-error "is reported only once" "" { target *-*-* } 7 } */
        /* { dg-error "function it appears in" "" { target *-*-* } 7 } */
        /* { dg-error "(parse|syntax) error" "" { target *-*-* } 7 } */

        return 0;
}

