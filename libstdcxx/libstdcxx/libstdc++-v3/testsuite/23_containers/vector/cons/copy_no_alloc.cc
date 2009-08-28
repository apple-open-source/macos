// 2009-01-06 Howard Hinnant

// Copyright (C) 2009
// Free Software Foundation, Inc.
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
// Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
// USA.

// vector copy construction of zero-sized vector should not allocate

#include <vector>
#include <new>

void* operator new(std::size_t size) throw(std::bad_alloc)
{
    throw std::bad_alloc();
    void* p = 0;
    return p;
}

int main() {
  std::vector<int> X;
  std::vector<int> Y(X);
}
