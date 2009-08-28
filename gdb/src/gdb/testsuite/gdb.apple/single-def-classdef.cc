#include "single-def.h"

test1abcdefg::test1abcdefg() :
	m_uint(0)
{
}

int foo ()
{
  return 5;
}

test1abcdefg::test1abcdefg(const test1abcdefg &rhs) 
{
  foo ();
}

test1abcdefg::~test1abcdefg()
{
}

bool 
test1abcdefg::empty() const
{
  if (m_uint == 0)
    return true;
  else
    return true;
}

size_t
test1abcdefg::size() const
{
  return (size_t) m_uint;
}

