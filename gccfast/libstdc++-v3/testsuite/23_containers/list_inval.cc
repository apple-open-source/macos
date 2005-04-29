// APPLE LOCAL file libstdc++ debug mode
// List iterator invalidation tests

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

#include <debug/list>
#include <iterator>
#include <testsuite_hooks.h>

using __gnu_debug::list;
using std::advance;

bool test = true;

// Assignment
void test01()
{
  list<int> v1;
  list<int> v2;

  v1.push_front(17);

  list<int>::iterator start = v1.begin();
  list<int>::iterator finish = v1.end();
  VERIFY(start._M_dereferenceable());
  VERIFY(!finish._M_dereferenceable() && !finish._M_singular());

  v1 = v2;
  VERIFY(start._M_singular());
  VERIFY(!finish._M_dereferenceable() && !finish._M_singular());

  finish = v1.end();
  v1.assign(v2.begin(), v2.end());
  VERIFY(!finish._M_dereferenceable() && !finish._M_singular());

  finish = v1.end();
  v1.assign(17, 42);
  VERIFY(!finish._M_dereferenceable() && !finish._M_singular());
}

// Resize
void test02()
{
  list<int> v(10, 17);

  list<int>::iterator before = v.begin();
  advance(before, 6);
  list<int>::iterator at = before;
  advance(at, 1);
  list<int>::iterator after = at;
  advance(after, 1);
  list<int>::iterator finish = v.end();

  // Shrink
  v.resize(7);
  VERIFY(before._M_dereferenceable());
  VERIFY(at._M_singular());
  VERIFY(after._M_singular());
  VERIFY(!finish._M_singular() && !finish._M_dereferenceable());
}

// Erase
void test03()
{
  list<int> v(20, 42);

  // Single element erase (middle)
  list<int>::iterator before = v.begin();
  list<int>::iterator at = before;
  advance(at, 3);
  list<int>::iterator after = at;
  at = v.erase(at);
  VERIFY(before._M_dereferenceable());
  VERIFY(at._M_dereferenceable());
  VERIFY(after._M_singular());

  // Single element erase (end)
  before = v.begin();
  at = before;
  after = at;
  ++after;
  at = v.erase(at);
  VERIFY(before._M_singular());
  VERIFY(at._M_dereferenceable());
  VERIFY(after._M_dereferenceable());

  // Multiple element erase
  before = v.begin();
  at = before;
  advance(at, 3);
  after = at;
  advance(after, 3);
  v.erase(at, after);
  VERIFY(before._M_dereferenceable());
  VERIFY(at._M_singular());

  // clear()
  before = v.begin();
  list<int>::iterator finish = v.end();
  VERIFY(before._M_dereferenceable());
  v.clear();
  VERIFY(before._M_singular());
  VERIFY(!finish._M_singular() && !finish._M_dereferenceable());
}

// Splice
void test04()
{
  list<int> l1(10, 17);
  list<int> l2(10, 42);
  
  list<int>::iterator start2 = l2.begin();
  list<int>::iterator end2 = start2;
  advance(end2, 5);
  list<int>::iterator after2 = end2;
  advance(after2, 2);
  
  l1.splice(l1.begin(), l2, start2, end2);
  VERIFY(start2._M_dereferenceable());
  VERIFY(end2._M_dereferenceable());
  VERIFY(after2._M_dereferenceable());
  VERIFY(start2._M_attached_to(&l1));
  VERIFY(end2._M_attached_to(&l2));
  VERIFY(after2._M_attached_to(&l2));
}

int main()
{
  test01();
  test02();
  test03();
  test04();
  return !test;
}
