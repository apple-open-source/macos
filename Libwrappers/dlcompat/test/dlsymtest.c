#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
int foo(int number);
typedef int (*testFNPtr)(int number);

int main (int argv, const char * argc[])
{
  int retCode=0;
  testFNPtr foofunc,foosym;
  foosym = (testFNPtr)dlsym(RTLD_NEXT,"foo");
  if (!foosym) printf("foosym RTLD_NEXT %s\n",dlerror());
  foofunc = (testFNPtr)dlfunc(RTLD_NEXT,"foo");
  if (!foofunc) printf("foofunc RTLD_NEXT %s\n",dlerror());
  if ((void*)foosym != (void*)foofunc)
  {
    retCode++;
    printf("RTLD_NEXT dlsym %x dlfunc %x\n",foosym,foofunc);
  }
#ifdef RTLD_DEFAULT  
  foosym = (testFNPtr)dlsym(RTLD_DEFAULT,"foo");
  if (!foosym) printf("foosym RTLD_DEFAULT %s\n",dlerror());
  foofunc = (testFNPtr)dlfunc(RTLD_DEFAULT,"foo");
  if (!foofunc) printf("foofunc RTLD_DEFAULT %s\n",dlerror());
  if ((void*)foosym != (void*)foofunc)
  {
    retCode++;
    printf("RTLD_DEFAULT dlsym %x dlfunc %x\n",foosym,foofunc);
  }
#endif  
#ifdef RTLD_SELF
  foosym = (testFNPtr)dlsym(RTLD_SELF,"foo");
  if (!foosym) printf("foosym RTLD_SELF %s\n",dlerror());
  foofunc = (testFNPtr)dlfunc(RTLD_SELF,"foo");
  if (!foofunc) printf("foofunc RTLD_SELF %s\n",dlerror());
  if ((void*)foosym != (void*)foofunc)
  {
    retCode++;
    printf("RTLD_SELF dlsym %x dlfunc %x\n",foosym,foofunc);
  }
#endif
  if (!retCode) printf("Okay, this actually works, cool, I am happy\n");
  else printf("Well, confirms a bug, makes me happy I was not wrong\n");
  return retCode;
}

int foo(int number)
{
printf("foo %s %s",__FILE__,__LINE__);
}