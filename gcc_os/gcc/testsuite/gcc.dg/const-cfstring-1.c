/* APPLE LOCAL file constant cfstrings */
/* Test whether the __builtin__CFStringMakeConstantString 
   "function" fails gracefully when handed a non-constant
   argument.  This will only work on MacOS X 10.1.2 and later.  */
/* Developed by Ziemowit Laski <zlaski@apple.com>.  */

/* { dg-options "-fconstant-cfstrings" } */
/* { dg-do compile } */

#import <CoreFoundation/CFString.h>

#ifdef __CONSTANT_CFSTRINGS__
#undef CFSTR
#define CFSTR(STR)  ((CFStringRef) __builtin___CFStringMakeConstantString (STR))
#endif

extern int cond;
extern const char *func(void);

int main(void) {
  CFStringRef s1 = CFSTR("Str1");
  CFStringRef s2 = CFSTR(cond? "Str2": "Str3"); /* { dg-error "CFString literal expression not constant" } */
  CFStringRef s3 = CFSTR(func());  /* { dg-error "CFString literal expression not constant" } */

  return 0;
}
