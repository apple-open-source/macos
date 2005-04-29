/* APPLE LOCAL file pointer casts */
/* Verify that -Wno-invalid-offsetof disables warning */
/* Author: Matt Austern <austern@apple.com> */
/* { dg-do compile } */
/* { dg-options "-Wno-invalid-offsetof" } */

struct X
{
  X() : x(3), y(4) { }
  int x, y;
};

typedef X* pX;

int yoff = int(&(pX(0)->y));
