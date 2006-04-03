/* MiG generated files use this a lot. */

void foo (void);

int
main ()
{
  typedef struct {
    char c;
  } mystruct;

  mystruct myvar;
  myvar.c = 'a';  
  myvar.c = 'b';  /* good stopping point in main */

  foo ();
}

void
foo ()
{
  typedef struct {
    int i;
  } mystruct;
  mystruct myvar;
  myvar.i = 10;
  myvar.i = 20; /* good stopping point in foo */
}

