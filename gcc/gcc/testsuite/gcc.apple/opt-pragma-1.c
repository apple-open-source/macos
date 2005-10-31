/* Test error handling of optimization pragmas. */
/* Radar 3124235 */
/* { dg-do compile } */
int outwit;  /* make the file non-empty */
#pragma optimization_level -1 /* { dg-warning "malformed '#pragma optimization_level" } */
#pragma optimization_level foo /* { dg-warning "malformed '#pragma optimization_level" } */
#pragma optimization_level 3.0 /* { dg-warning "malformed '#pragma optimization_level" } */
#pragma optimization_level 3 extra /* { dg-warning "junk at end of '#pragma optimization_level" } */
#pragma optimization_level 0x4
#pragma optimize_for_size on
#pragma optimize_for_size foo  /* { dg-warning "malformed '#pragma optimize_for_size" } */
#pragma optimization_level reset
#pragma optimization_level reset
#pragma optimization_level reset
#pragma optimization_level reset /* { dg-warning "optimization pragma stack underflow" } */
