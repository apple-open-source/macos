#include <string.h> // strcmp()

#include "cpp_dialect.h"

int dcc_seen_opt_x = 0;
char *dcc_opt_x_ext = NULL;

struct assoc_t;
struct assoc_t
{
  char *key;
  char *val;
};
typedef struct assoc_t assoc_t;

static assoc_t dialect_tbl[] =
{
    { "c",             "cpp-output",        }, /**/
    { "c++",           "c++-cpp-output",    },
    { "objective-c",   "objc-cpp-output",   }, /**/
    { "objective-c++", "objc++-cpp-output", }, /**/

    { NULL, NULL }
};

static assoc_t ext_tbl[] =
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

char *dcc_dialect_lookup(char *key)
{
  return tbl_lookup(dialect_tbl, key);
}

char *dcc_ext_lookup(char *key)
{
  return tbl_lookup(ext_tbl, key);
}

void dcc_fix_opt_x(char **argv)
{
    int i;
    for(i = 0; NULL != argv[i]; i++)
    {
        if(!strcmp(argv[i], "-x"))
        {
            char *dialect;
            if(dialect = dcc_dialect_lookup(argv[i + 1]))
            {
                argv[i + 1] = dialect;
            }
        }
    }
    return;
}
