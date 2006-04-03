typedef enum {a = 1, b = 4294967294, c = 4294967295} myenum;

int
main (int argc, char **argv)
{  
  myenum myvar;
 
  if (argc > 0)
    myvar = b;
  else
    myvar = c;

  return myvar;  /* good stopping point in main */
}
