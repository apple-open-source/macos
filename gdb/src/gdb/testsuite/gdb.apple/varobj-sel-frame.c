int bar (int in_val)
{
  return in_val + 2;
}

int foo (int my_var)
{
  int temp = my_var;

  if (temp > 0)
    {
      int my_var = temp * 3;
      temp = bar (my_var);
    }
    
  return my_var * 10;
}

int main ()
{
  int my_var = 5;

  my_var = foo (my_var * 5);

  return 0;
}
