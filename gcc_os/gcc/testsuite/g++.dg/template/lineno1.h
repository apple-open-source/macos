template <int x> int do_something () 
{
  return x;
}
template <int y> int do_something_2 (void)
{
  return do_something<y + 2>();
}  
