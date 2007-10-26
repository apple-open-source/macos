/* APPLE LOCAL file radar 4430340 */
/* { dg-do compile } */
int a[1/0]; /* { dg-warning "division by zero" } */
            /* { dg-error "error: ISO C90 forbids array 'a' whose size can't be evaluated" "ISO" { target *-*-* } 3 } */
            /* { dg-error "error: storage size of 'a' isn't constant" "storage" { target *-*-* } 3 } */
/* APPLE LOCAL file radar 4430340 */
