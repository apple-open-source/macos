const int second_const_int = 5;
const int second_const_doesnt_match = 6;
const char *second_const_char = "I am a constant";
int second_global_int = 6;
static int second_static_int = 7;
static int second_static_doesnt_match = 8;

int foo ()
{
  /* Use all the variables so -gused won't strip them.  */
  if (strcmp (second_const_char, "something") != 0)
    return second_static_doesnt_match * second_const_doesnt_match 
      * second_const_int * second_global_int * second_static_int;
  else
    return 0;
}
