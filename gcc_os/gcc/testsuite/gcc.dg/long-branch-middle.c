/* { dg-do dummy } */

dummy ()
{
  /* Create large (128M) initialized area filled with garbage.  */
  __asm__ ("	.fill	32 * 1024 * 1024, 4, 0x12345678\n");
}
