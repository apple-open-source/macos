struct my_struct
{
  int one;
  int other;
};

class Base
{
public:
  Base (int in_my_var) : my_var (in_my_var) {}
  virtual void incr_me (int amount)
  {
    my_var += amount;
  }
private:
  int my_var;
};

class Derived : public Base
{
public:
  Derived (int in_my_var, int in_one, int in_other) : Base (in_my_var)
  {
    my_struct_ptr = new (my_struct);
    my_struct_ptr->one = in_one;
    my_struct_ptr->other = in_other;
  };
private:
  struct my_struct *my_struct_ptr;
};

int 
pass_ref (Derived &input)
{
  input.incr_me (5);
  return 10;
}

int 
pass_ref_ptr (Derived * &input_ptr)
{
  input_ptr->incr_me (5);
  return 10;
}

int 
pass_base_ref (Base &input)
{
  input.incr_me (5);
  return 10;
}

int 
main ()
{
  Derived my_derived (5, 10, 20);
  Derived *derived_ptr = new Derived (5, 10, 20);

  pass_ref (my_derived);
  pass_ref_ptr (derived_ptr);
  pass_base_ref (my_derived);
  return 0;
}
