// APPLE LOCAL file libstdc++ debug mode

// Copyright (C) 2003 Free Software Foundation
// { dg-do compile }

template<typename T>
class __attribute__((__link_name__("bar"))) foo {};

void do_it(const foo<int>&) {}

// { dg-final { scan-assembler "\n_?_Z5do_itRK3barIiE\[: \t\n\]" } }
