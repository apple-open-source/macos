/* Simple arithmetic for numbers greater than a unsigned long, for GNU tar.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Also, see comments at beginning of arith.c.  */

#define BITS_PER_BYTE 8		/* number of bits in each sizeof unit */
#define BITS_PER_TARLONG 42	/* wanted number of bits in each tarlong */

/* In all cases, tarlong is the proper type for a big number.

   For simulated arithmetic, SUPERDIGIT is the base, TARLONG_FORMAT is the
   format to print a single super-digit filled with zeroes to the left, and
   BITS_PER_SUPERDIGIT is the smallest number of bits required to fully
   represent each super-digit.  LONGS_PER_TARLONG says how many longs are
   required for a full tarlong, and SIZEOF_TARLONG is the size of a tarlong
   in bytes.

   For straight compiler arithmetic, SUPERDIGIT is zero and TARLONG_FORMAT
   is the format to directly print a tarlong (without zero-filling).

   The values of SIZEOF_LONG_LONG and SIZEOF_UNSIGNED_LONG, below, are
   obtained through the configuration process.  */

#if BITS_PER_BYTE * SIZEOF_UNSIGNED_LONG >= BITS_PER_TARLONG
# define SUPERDIGIT 0
# define TARLONG_FORMAT "%lu"
typedef unsigned long tarlong;
#else
# if BITS_PER_BYTE * SIZEOF_LONG_LONG >= BITS_PER_TARLONG + 1
#  define SUPERDIGIT 0
#  define TARLONG_FORMAT "%llu"
typedef unsigned long long tarlong;
# else
#  if BITS_PER_BYTE * SIZEOF_UNSIGNED_LONG >= 64
#   define SUPERDIGIT 1000000000L
#   define BITS_PER_SUPERDIGIT 29
#   define TARLONG_FORMAT "%09uld"
#  else
#   if BITS_PER_BYTE * SIZEOF_UNSIGNED_LONG >= 32
#    define SUPERDIGIT 10000L
#    define BITS_PER_SUPERDIGIT 14
#    define TARLONG_FORMAT "%04uld"
#   endif
#  endif
# endif
#endif

#if SUPERDIGIT

# define LONGS_PER_TARLONG \
    ((BITS_PER_TARLONG + BITS_PER_SUPERDIGIT - 1) / BITS_PER_SUPERDIGIT)
# define SIZEOF_TARLONG (LONGS_PER_TARLONG * sizeof (unsigned long))

/* The NEC EWS 4.2 C compiler gets confused by a pointer to a typedef that
   is an array.  So we wrap the array into a struct.  (Pouah!)  */

struct tarlong
{
  unsigned long digit[LONGS_PER_TARLONG];
};

typedef struct tarlong tarlong;

int zerop_tarlong_helper PARAMS ((unsigned long *));
int lessp_tarlong_helper PARAMS ((unsigned long *, unsigned long *));
void clear_tarlong_helper PARAMS ((unsigned long *));
void add_to_tarlong_helper PARAMS ((unsigned long *, unsigned long));
void mult_tarlong_helper PARAMS ((unsigned long *, unsigned long));
void print_tarlong_helper PARAMS ((unsigned long *, FILE *));

# define zerop_tarlong(Accumulator) \
   zerop_tarlong_helper (&(Accumulator).digit[0])

# define lessp_tarlong(First, Second) \
   lessp_tarlong_helper (&(First).digit[0], &(Second).digit[0])

# define clear_tarlong(Accumulator) \
   clear_tarlong_helper (&(Accumulator).digit[0])

# define add_to_tarlong(Accumulator, Value) \
   add_to_tarlong_helper (&(Accumulator).digit[0], (unsigned long) (Value))

# define mult_tarlong(Accumulator, Value) \
   mult_tarlong_helper (&(Accumulator).digit[0], (unsigned long) (Value))

# define print_tarlong(Accumulator, File) \
   print_tarlong_helper (&(Accumulator).digit[0], (File))

#else /* not SUPERDIGIT */

# define zerop_tarlong(Accumulator) \
   ((Accumulator) == 0)

# define lessp_tarlong(First, Second) \
   ((First) < (Second))

# define clear_tarlong(Accumulator) \
   ((Accumulator) = 0)

# define add_to_tarlong(Accumulator, Value) \
   ((Accumulator) += (Value))

# define mult_tarlong(Accumulator, Value) \
   ((Accumulator) *= (Value))

# define print_tarlong(Accumulator, File) \
   (fprintf ((File), TARLONG_FORMAT, (Accumulator)))

#endif /* not SUPERDIGIT */
