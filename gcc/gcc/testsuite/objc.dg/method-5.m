/* APPLE LOCAL file Panther ObjC enhancements */
/* Check if sending messages to "underspecified" objects is handled gracefully.  */
/* Author: Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-do compile } */

@class UnderSpecified;
typedef struct NotAClass {
  int a, b;
} NotAClass;

void foo(UnderSpecified *u, NotAClass *n) {
  [n nonexistent_method];    /* { dg-warning "invalid receiver type" } */
  /* { dg-warning "cannot find method .\\-nonexistent_method.. return type .id. assumed" "" { target *-*-* } 12 } */
  [NotAClass nonexistent_method]; /* { dg-error ".NotAClass. is not an Objective\\-C class name or alias" } */
  [u nonexistent_method]; /* { dg-warning ".UnderSpecified. may not respond to .\\-nonexistent_method." } */
  /* { dg-warning "cannot find method .\\-nonexistent_method.. return type .id. assumed" "" { target *-*-* } 15 } */
  [UnderSpecified nonexistent_method]; /* { dg-warning ".UnderSpecified. may not respond to .\\+nonexistent_method." } */
  /* { dg-warning "cannot find method .\\+nonexistent_method.. return type .id. assumed" "" { target *-*-* } 17 } */
}
