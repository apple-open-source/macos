/* APPLE LOCAL file Negative C++ test case 30 Jul 2002 */
/* Origin: Matt Austern <austern@apple.com> */
/* { dg-do compile } */
/* { dg-options "-fapple-kext" } */

struct B1 { };			/* ok */
struct B2 { };			/* ok */
struct D1 : B1 { };		/* ok */
struct D2 : B1, B2 { };		/* { dg-error "multiple bases, conflicts" } */
struct D3 : virtual B1 { };     /* { dg-error "virtual base, conflicts" } */
