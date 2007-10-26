/* APPLE LOCAL file mainline 2006-02-13 4433453 */
/* { dg-options "-gdwarf-2 -dA" } */
/* { dg-final { scan-assembler "\"id.0\".*DW_AT_name" } } */
@interface foo
  id x;
@end
