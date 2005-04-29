#include <string.h> // strcmp()

#include "cpp_dialect.h"

int seen_opt_x = 0;
char *opt_x_ext = NULL;

struct assoc_t;
struct assoc_t
{
  char *key;
  char *val;
};
typedef struct assoc_t assoc_t;

assoc_t dialect_tbl[] =
{
    { "c",             "cpp-output",        }, /**/
    { "c++",           "c++-cpp-output",    },
    { "objective-c",   "objc-cpp-output",   }, /**/
    { "objective-c++", "objc++-cpp-output", }, /**/

    { NULL, NULL }
};

assoc_t ext_tbl[] =
{
    { "c",                 ".i"   }, /**/
    { "cpp-output",        ".i"   }, /**/

    { "c++",               ".ii"  },
    { "c++-cpp-output",    ".ii"  },

    { "objective-c",       ".mi"  }, /**/
    { "objc-cpp-output",   ".mi"  }, /**/

    { "objective-c++",     ".mii" }, /**/
    { "objc++-cpp-output", ".mii" }, /**/

    { NULL, NULL }
};

static char *tbl_lookup(assoc_t *tbl, char *key)
{
    int i = 0;
    while (tbl[i].key) {
      if (!strcmp(tbl[i].key, key)) {
            return tbl[i].val;
      }
        i++;
    }
    return NULL;
}

char *dialect_lookup(char *key)
{
  return tbl_lookup(dialect_tbl, key);
}

char *ext_lookup(char *key)
{
  return tbl_lookup(ext_tbl, key);
}
