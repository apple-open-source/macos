// APPLE LOCAL file libstdc++ debug mode
// Deque iterator invalidation tests

// Copyright (C) 2003 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

#include <debug/deque>
#include <testsuite_hooks.h>

using __gnu_debug::deque;

bool test = true;

// Assignment
void test01()
{
  deque<int> v1;
  deque<int> v2;

  deque<int>::iterator i = v1.end();
  VERIFY(!i._M_dereferenceable() && !i._M_singular());

  v1 = v2;
  VERIFY(i._M_singular());
  
  i = v1.end();
  v1.assign(v2.begin(), v2.end());
  VERIFY(i._M_singular());

  i = v1.end();
  v1.assign(17, 42);
  VERIFY(i._M_singular());
}

// Resize
void test02()
{
  deque<int> v(10, 17);

  deque<int>::iterator before = v.begin() + 6;
  deque<int>::iterator at = before + 1;
  deque<int>::iterator after = at + 1;

  // Shrink
  v.resize(7);
  VERIFY(before._M_dereferenceable());
  VERIFY(at._M_singular());
  VERIFY(after._M_singular());

  // Grow
  before = v.begin() + 6;
  v.resize(17);
  VERIFY(before._M_singular());
}

// Insert
void test03()
{
  deque<int> v(10, 17);

  // Insert a single element
  deque<int>::iterator before = v.begin() + 6;
  deque<int>::iterator at = before + 1;
  deque<int>::iterator after = at;
  at = v.insert(at, 42);
  VERIFY(before._M_singular());
  VERIFY(at._M_dereferenceable());
  VERIFY(after._M_singular());

  // Insert multiple copies
  before = v.begin() + 6;
  at = before + 1;
  v.insert(at, 3, 42);
  VERIFY(before._M_singular());
  VERIFY(at._M_singular());

  // Insert iterator range
  static int data[] = { 2, 3, 5, 7 };
  before = v.begin() + 6;
  at = before + 1;
  v.insert(at, &data[0], &data[0] + 4);
  VERIFY(before._M_singular());
  VERIFY(at._M_singular());
}

// Erase
void test04()
{
  deque<int> v(20, 42);

  // Single element erase (middle)
  deque<int>::iterator before = v.begin();
  deque<int>::iterator at = before + 3;
  deque<int>::iterator after = at;
  at = v.erase(at);
  VERIFY(before._M_singular());
  VERIFY(at._M_dereferenceable());
  VERIFY(after._M_singular());

  // Single element erase (end)
  before = v.begin();
  at = before;
  after = at + 1;
  at = v.erase(at);
  VERIFY(before._M_singular());
  VERIFY(at._M_dereferenceable());
  VERIFY(after._M_dereferenceable());

  // Multiple element erase
  before = v.begin();
  at = before + 3;
  v.erase(at, at + 3);
  VERIFY(before._M_singular());
  VERIFY(at._M_singular());

  // clear()
  before = v.begin();
  VERIFY(before._M_dereferenceable());
  v.clear();
  VERIFY(before._M_singular());
}

int main()
{
  test01();
  test02();
  test03();
  test04();
  return !test;
}
