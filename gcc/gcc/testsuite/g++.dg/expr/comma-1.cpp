/* APPLE LOCAL file AltiVec */
/* Test if casting a comma expression works as expected; Apple's AltiVec syntax
   handling actually managed to break this.  */
/* { dg-do run } */

extern void abort(void);
extern int strcmp(const char *, const char *);
#define CHECK_IF(expr) if(!(expr)) abort()
 
typedef void* (*MyProcPtr)(short argument);


static char *my_convert(int n) {
  static char result[8];
  char *p = result;
  while(n) {
    *p++ = (n % 10) + '0';
    n /= 10;
  }
  *p = 0;
  return result;
}

int main(void)
{
    MyProcPtr procPtr = (MyProcPtr)my_convert;
    short   aNumber = 123;
    char    *newBuff =(char*)(((void*)0),*((MyProcPtr)(procPtr)))(aNumber + 3);
    
    CHECK_IF(!strcmp(newBuff, "621"));
    return 0;
}

