/* APPLE LOCAL file -findirect-virtual-calls 15 Oct 2002 */
/* Radar 3008388: Positive C++ test case. */
/* Origin: Matt Austern <austern@apple.com> */
/* { dg-do compile } */
/* { dg-options "-fapple-kext" } */

struct B1 { };			/* ok */
struct B2 { };			/* ok */
struct D1 : B1 { };		/* ok */
struct D2 : B1, B2 { };		/* ok */
struct D3 : virtual B1 { };     /* ok */
