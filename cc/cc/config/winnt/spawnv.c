/* This is a kludge to get around the Microsoft C spawn functions' propensity
   to remove the outermost set of double quotes from all arguments.  */

#define index(s,c) strchr((s),(c))

extern char *malloc ();

void
fix_argv (argv, cmdname)
  char **argv;
  char **cmdname;
{
  static char sh_chars[] = "\"";

  int i, len;
  char *new_argv;
  char *p, *ap;

  for (i=1; argv[i]; i++)
    {

      len = strlen (argv[i]);
      new_argv = malloc (2*len+3);
      ap = new_argv;

      for (p = argv[i]; *p != '\0'; ++p)
        {
          if (index (sh_chars, *p) != 0)
            *ap++ = '\\';
          *ap++ = *p;
        }
      *ap = '\0';
      argv[i] = new_argv;
    }

  if (cmdname && *cmdname) {
    len = strlen(*cmdname);
    new_argv = malloc(len+1);
    for(i=0; i<=len; i++) {
       new_argv[i] = ('/' == (*cmdname)[i] ? '\\' : (*cmdname)[i]);
    }
    *cmdname = new_argv;
  }
}

int __spawnv (mode, cmdname, argv)
  int mode;
  const char *cmdname;
  char **argv;
{
  fix_argv (argv, &cmdname);
  _spawnv (mode, cmdname, argv);
}

int __spawnvp (mode, cmdname, argv)
  int mode;
  const char *cmdname;
  char **argv;
{
  fix_argv (argv, &cmdname);
  _spawnvp (mode, cmdname, argv);
}

int spawnve (mode, cmdname, argv, envp)
  int mode;
  const char *cmdname;
  char **argv;
  const char *const *envp;
{
  fix_argv (argv, &cmdname);
  _spawnve (mode, cmdname, argv, envp);
}

int __spawnvpe (mode, cmdname, argv, envp)
  int mode;
  const char *cmdname;
  char **argv;
  const char *const *envp;
{
  fix_argv (argv, &cmdname);
  _spawnvpe (mode, cmdname, argv, envp);
}

