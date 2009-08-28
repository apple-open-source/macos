#include <stdio.h>
#include <Block.h>

struct test1
{
  char test1_char;
  int test1_int;
};

int called_from_block (int in_value)
{
  return in_value * 2;
}

int (^(block_maker (int in_ref_value, char *strptr))) (int, char *)
{
  __block int by_ref = in_ref_value;
  int by_value = -55;
  int test_value;

  int (^blockptr) (int, char *) = ^(int inval, char *strptr) {
    int local_var;
    local_var = called_from_block (inval);
    local_var += by_value;
    by_ref++;
    /* Breakpoint in int return block */
    return  by_ref + local_var;
  };

  return Block_copy (blockptr);
}

int 
block_taker (int (^myBlock)(int, char *), int other_input, char *instr)
{
  return 5 * myBlock (other_input, instr);
}

struct test1
block_taker_struct (struct test1 (^myBlock) (int), int test1_int)
{
  return myBlock (test1_int);
}

struct test1 (^(block_maker_struct (int in_ref))) (int)
{
  __block struct test1 by_ref;
  int by_value = 2 * in_ref;
  struct test1 test_value;
  by_ref.test1_int = 1;
  by_ref.test1_char = 'a';

  struct test1 (^blockptr) (int) = ^(int inint) {
    struct test1 my_struct;
    called_from_block (0);
    if (by_value > 1)
      my_struct.test1_char = 'a';
    else 
      my_struct.test1_char = 'b';
    my_struct.test1_int = inint + by_ref.test1_int;
    by_ref.test1_int++;
    /* Breakpoint in struct return block */
    return my_struct;
  };

  return Block_copy (blockptr);
}

int main (int argc, char **argv)
{
  int int_ret;
  struct test1 test1_ret;

  int (^block_ptr) (int, char *);
  struct test1 (^struct_block_ptr) (int);

  /* Breakpoint before making blocks */
  block_ptr = (int (^) (int, char *)) block_maker (100, "some string");
  struct_block_ptr  = (struct test1 (^) (int)) block_maker_struct (100);

  /* Breakpoint after making blocks */
  int_ret = block_taker (block_ptr, 5, "some string");

  /* Breakpoint after calling blockptr */
  test1_ret = block_taker_struct (struct_block_ptr, 10);

  return 0;
}
