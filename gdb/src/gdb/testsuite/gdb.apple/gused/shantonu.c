#include <stdint.h>

typedef uint64_t event64_t;

struct thread {
  event64_t               wait_event;                     /* wait queue event */
};

struct thread foo = {  0 };

int
main ()
{
  foo.wait_event = 5;
  puts ("");  /* a good place to put a breakpoint */
  return 0;
}
