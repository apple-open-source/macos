#include <mach-o/dyld.h> // _dyld_func_lookup()
#include <stdint.h> // uint32_t

static
uint32_t
uint32_from_binstr(const char *binstr)
{
  uint32_t total = 0;
  uint32_t colmult = 1; // 2^0
  size_t binstrlen = 0;
  while((char)0 != binstr[binstrlen])
  {
    if(('0' == binstr[binstrlen])
    || ('1' == binstr[binstrlen]))
    {
      if(32 > binstrlen)
	{ binstrlen++; }
      else
	{ return 0; }
    }
    else
      { return 0; }
  }
  uint32_t i;
  for(i = binstrlen; i > 0; i--)
  {
    uint32_t bitval = binstr[i - 1] - '0';
    uint32_t subtotal = bitval * colmult;
    total += subtotal;
    colmult *= 2; // 2^n
#if 0
    printf("%u x 2^% 2u = % 10u [total = % 10u]\n",
	   binstr[i - 1] - '0', binstrlen - i, subtotal, total);
#endif
  }
  return total;
}

__attribute__((noreturn))
void _start(int argc, const char* argv[])
{
  uint32_t flags = uint32_from_binstr(argv[argc - 1]);
  argv[argc - 1] = NULL;
  argc--;
  void (*update_prebinding)(int pathCount, const char* paths[], uint32_t flags) __attribute__((noreturn));
  _dyld_func_lookup("__dyld_update_prebinding", (void**)&update_prebinding);
  update_prebinding(argc, argv, flags);
}
