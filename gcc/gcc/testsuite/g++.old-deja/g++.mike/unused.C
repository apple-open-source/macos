// APPLE LOCAL file mainline lookup_name 4125055
// { dg-do compile { target *-*-darwin* } }
// { dg-options { -Wunused-parameter } }
// Radar 4125055

void foo(int x) {
#pragma unused ( x )
}
