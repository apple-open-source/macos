/*
 * States definitions file for States tests.
 * Copyright (c) 1997 Markku Rossi.
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Initializations.
 */

a_variable = "false";

start
{
  check_startrules ();
  check_namerules ();
}

startrules
{
  /Test startrules\./	test_startrules;
}

namerules
{
  /\.st$/	test_namerules;
}

sub ok ()
{
  print ("ok");
}

sub fail ()
{
  print ("fail");
}


/*
 * Test states.
 */

state skip_input
{
  /[^\\\/]+/ {
    /* NOP */
  }
  /./ {
    /* NOP */
  }
}

state test_startrules
{
  BEGIN {
    print ("test_startrules");
    call (skip_input);
  }
}

state test_namerules
{
  BEGIN {
    print ("test_namerules");
    call (skip_input);
  }
}

state test_optionstate
{
  BEGIN {
    print ("test_optionstate");
    call (skip_input);
  }
}

state test_first_match
{
  /aaaa/ {
    ok ();
    call (skip_input);
  }
  /[ab]+/ {
    fail ();
    call (skip_input);
  }
}

state test_case_insensitive_regexps
{
  /aaaa/i {
    ok ();
    call (skip_input);
  }
}

state test_vardef
{
  BEGIN {
    print (a_variable);
    call (skip_input);
  }
}

state test_exprs
{
  BEGIN {

    /* Postfix add. */
    a = 1;
    if (a++ != 1)
      fail ();
    if (a++ != 2)
      fail ();
    if (a++ != 3)
      fail ();

    /* Postfix sub. */
    if (a-- != 4)
      fail ();
    if (a-- != 3)
      fail ();
    if (a-- != 2)
      fail ();

    /* Prefix add. */
    a = 1;
    if (++a != 2)
      fail ();
    if (++a != 3)
      fail ();

    /* Prefix sub. */
    if (--a != 2)
      fail ();
    if (--a != 1)
      fail ();

    /* += */
    a = 0;
    a += 5;
    if (a != 5)
      fail ();

    /* -= */
    a -= 3;
    if (a != 2)
      fail ();

    /* *= */
    a *= 2;
    if (a != 4)
      fail ();

    /* div= */
    a div= 2;

    if (a != 2)
      fail ();

    call (skip_input);
    ok ();
  }
}

state test_primconcat
{
  BEGIN {
    if (strcmp (concat ("a", "b", "c"), "abc") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primfloat
{
  BEGIN {
    if (float (/f/) != 0.0)
      fail ();
    if (float (list (1, 2, 3)) != 3.0)
      fail ();
    if (float ("1") != 1.0)
      fail ();
    if (float ("1.34") != 1.34)
      fail ();
    if (float ("") != 0.0)
      fail ();
    if (float (1) != 1.0)
      fail ();
    if (float (1.1) != 1.1)
      fail ();
    call (skip_input);
  }
}

state test_primgetenv
{
  BEGIN {
    if (strcmp (getenv ("STATES_DATA"), "ok") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primint
{
  BEGIN {
    if (int (/a/) != 0)
      fail ();
    if (int (list (1, 2, 3, 4)) != 4)
      fail ();
    if (int ("1") != 1)
      fail ();
    if (int ("1.5") != 1)
      fail ();
    if (int ("") != 0)
      fail ();
    if (int (3) != 3)
      fail ();
    if (int (1.1) != 1)
      fail ();
    call (skip_input);
  }
}

state test_primlength
{
  BEGIN {
    if (length ("ab") != 2)
      fail ();
    if (length (list (1, 2, "3", /4/)) != 4)
      fail ();
    call (skip_input);
  }
}

state test_primlist
{
  BEGIN {
    lst = list (1, "2", /3/, 4);
    if (lst[0] != 1)
      fail ();
    if (lst[3] != 4)
      fail ();
    call (skip_input);
  }
}

state test_primprint
{
  BEGIN {
    print ("ok", 1, /2/);
    call (skip_input);
  }
}

state test_primregexp
{
  BEGIN {
    re = regexp (".*");
    if (!regmatch ("abcd", re))
      fail ();
    call (skip_input);
  }
}

state test_primregexp_syntax
{
  BEGIN {
    regexp_syntax ('-', 'w');
    if (regmatch ("foo-bar", /\bbar\b/))
      fail ();
    call (skip_input);
  }
}

state test_primregmatch
{
  BEGIN {
    if (!regmatch ("abcde foo bar", /[a-z]+ ([a-z]+)/))
      fail ();
    if (strcmp ($0, "abcde foo") != 0)
      fail ();
    if (strcmp ($1, "foo") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primregsub
{
  BEGIN {
    if (strcmp (regsub ("a.b.c.d", /\./, "_"), "a_b.c.d") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primregsuball
{
  BEGIN {
    if (strcmp (regsuball ("a.b.c.d", /\./, "_"), "a_b_c_d") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primsprintf
{
  BEGIN {
    str = sprintf ("%d: foo %s %.2f", 1, "bar", 1.0);
    if (strcmp (str, "1: foo bar 1.00") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primstrcmp
{
  BEGIN {
    if (strcmp ("a", "b") != -1)
      fail ();
    if (strcmp ("aa", "a") != 1)
      fail ();
    if (strcmp ("a", "a") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primstring
{
  BEGIN {
    str = concat (string (1), string ("a"));
    if (strcmp (str, "1a") != 0)
      fail ();
    call (skip_input);
  }
}

state test_primstrncmp
{
  BEGIN {
    if (strncmp ("a", "ab", 1) != 0)
      fail ();
    if (strncmp ("aaa", "a", 2) != 1)
      fail ();
    call (skip_input);
  }
}

state test_primsubstring
{
  BEGIN {
    if (strcmp (substring ("abcdef", 1, 3), "bc") != 0)
      fail ();
    call (skip_input);
  }
}


/*
Local Variables:
mode: c
End:
*/
