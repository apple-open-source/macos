/* APPLE LOCAL file __block assign sequence point 6639533 */
/* { dg-do compile } */
/* { dg-options "-Wall -fblocks" } */

int foo() {
    __block int retval;
    retval = 0;
    return retval;
}
