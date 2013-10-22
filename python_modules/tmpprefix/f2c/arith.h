#if defined(__x86_64__)
#define IEEE_8087
#define Arith_Kind_ASL 1
#define Long int
#define Intcast (int)(long)
#define Double_Align
#define X64_bit_pointers
#define NANCHECK
#define QNaN0 0x0
#define QNaN1 0xfff80000
#elif defined(__i386__)
#define IEEE_8087
#define Arith_Kind_ASL 1
#define NANCHECK
#define QNaN0 0x0
#define QNaN1 0xfff80000
#else
#error Unknown architecture
#endif
