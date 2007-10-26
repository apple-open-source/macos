/* PowerPC-specific test program for setting of registers. */

register long glob1 asm ("r14");

register long long glob2 asm ("r20");

main()
{
  glob1 = 0x1234;
  glob2 = 0x123456787abcdef0LL;
  return 0;
}
