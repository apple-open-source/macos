/* APPLE LOCAL file pointer casts */
/* Verify that offsetof warns if given a non-POD */
/* Author: Matt Austern <austern@apple.com> */
/* { dg-do compile } */

struct X
{
  X() : x(3), y(4) { }
  int x, y;
};

typedef X* pX;

int yoff = int(&(pX(0)->y)); /* { dg-warning "invalid access" "" } */
/* { dg-warning "macro was used incorrectly" "" { target *-*-* } 14 } */
