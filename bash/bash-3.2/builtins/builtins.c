/* builtins.c -- the built in shell commands. */

/* This file is manufactured by ./mkbuiltins, and should not be
   edited by hand.  See the source to mkbuiltins for details. */

/* Copyright (C) 1987-2002 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bash is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111 USA. */

/* The list of shell builtins.  Each element is name, function, flags,
   long-doc, short-doc.  The long-doc field contains a pointer to an array
   of help lines.  The function takes a WORD_LIST *; the first word in the
   list is the first arg to the command.  The list has already had word
   expansion performed.

   Functions which need to look at only the simple commands (e.g.
   the enable_builtin ()), should ignore entries where
   (array[i].function == (sh_builtin_func_t *)NULL).  Such entries are for
   the list of shell reserved control structures, like `if' and `while'.
   The end of the list is denoted with a NULL name field. */

#include "../builtins.h"
#include "builtext.h"
#include "bashintl.h"

struct builtin static_shell_builtins[] = {
#if defined (ALIAS)
  { "alias", alias_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | ASSIGNMENT_BUILTIN, alias_doc,
     "alias [-p] [name[=value] ... ]", (char *)NULL },
#endif /* ALIAS */
#if defined (ALIAS)
  { "unalias", unalias_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, unalias_doc,
     "unalias [-a] name [name ...]", (char *)NULL },
#endif /* ALIAS */
#if defined (READLINE)
  { "bind", bind_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, bind_doc,
     "bind [-lpvsPVS] [-m keymap] [-f filename] [-q name] [-u name] [-r keyseq] [-x keyseq:shell-command] [keyseq:readline-function or readline-command]", (char *)NULL },
#endif /* READLINE */
  { "break", break_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, break_doc,
     "break [n]", (char *)NULL },
  { "continue", continue_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, continue_doc,
     "continue [n]", (char *)NULL },
  { "builtin", builtin_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, builtin_doc,
     "builtin [shell-builtin [arg ...]]", (char *)NULL },
#if defined (DEBUGGER)
  { "caller", caller_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, caller_doc,
     "caller [EXPR]", (char *)NULL },
#endif /* DEBUGGER */
  { "cd", cd_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, cd_doc,
     "cd [-L|-P] [dir]", (char *)NULL },
  { "pwd", pwd_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, pwd_doc,
     "pwd [-LP]", (char *)NULL },
  { ":", colon_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, colon_doc,
     ":", (char *)NULL },
  { "true", colon_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, true_doc,
     "true", (char *)NULL },
  { "false", false_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, false_doc,
     "false", (char *)NULL },
  { "command", command_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, command_doc,
     "command [-pVv] command [arg ...]", (char *)NULL },
  { "declare", declare_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | ASSIGNMENT_BUILTIN, declare_doc,
     "declare [-afFirtx] [-p] [name[=value] ...]", (char *)NULL },
  { "typeset", declare_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | ASSIGNMENT_BUILTIN, typeset_doc,
     "typeset [-afFirtx] [-p] name[=value] ...", (char *)NULL },
  { "local", local_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | ASSIGNMENT_BUILTIN, local_doc,
     "local name[=value] ...", (char *)NULL },
#if defined (V9_ECHO)
  { "echo", echo_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, echo_doc,
     "echo [-neE] [arg ...]", (char *)NULL },
#endif /* V9_ECHO */
#if !defined (V9_ECHO)
  { "echo", echo_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, echo_doc,
     "echo [-n] [arg ...]", (char *)NULL },
#endif /* !V9_ECHO */
  { "enable", enable_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, enable_doc,
     "enable [-pnds] [-a] [-f filename] [name ...]", (char *)NULL },
  { "eval", eval_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, eval_doc,
     "eval [arg ...]", (char *)NULL },
  { "getopts", getopts_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, getopts_doc,
     "getopts optstring name [arg]", (char *)NULL },
  { "exec", exec_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, exec_doc,
     "exec [-cl] [-a name] file [redirection ...]", (char *)NULL },
  { "exit", exit_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, exit_doc,
     "exit [n]", (char *)NULL },
  { "logout", logout_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, logout_doc,
     "logout", (char *)NULL },
#if defined (HISTORY)
  { "fc", fc_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, fc_doc,
     "fc [-e ename] [-nlr] [first] [last] or fc -s [pat=rep] [cmd]", (char *)NULL },
#endif /* HISTORY */
#if defined (JOB_CONTROL)
  { "fg", fg_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, fg_doc,
     "fg [job_spec]", (char *)NULL },
#endif /* JOB_CONTROL */
#if defined (JOB_CONTROL)
  { "bg", bg_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, bg_doc,
     "bg [job_spec ...]", (char *)NULL },
#endif /* JOB_CONTROL */
  { "hash", hash_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, hash_doc,
     "hash [-lr] [-p pathname] [-dt] [name ...]", (char *)NULL },
#if defined (HELP_BUILTIN)
  { "help", help_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, help_doc,
     "help [-s] [pattern ...]", (char *)NULL },
#endif /* HELP_BUILTIN */
#if defined (HISTORY)
  { "history", history_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, history_doc,
     "history [-c] [-d offset] [n] or history -awrn [filename] or history -ps arg [arg...]", (char *)NULL },
#endif /* HISTORY */
#if defined (JOB_CONTROL)
  { "jobs", jobs_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, jobs_doc,
     "jobs [-lnprs] [jobspec ...] or jobs -x command [args]", (char *)NULL },
#endif /* JOB_CONTROL */
#if defined (JOB_CONTROL)
  { "disown", disown_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, disown_doc,
     "disown [-h] [-ar] [jobspec ...]", (char *)NULL },
#endif /* JOB_CONTROL */
  { "kill", kill_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, kill_doc,
     "kill [-s sigspec | -n signum | -sigspec] pid | jobspec ... or kill -l [sigspec]", (char *)NULL },
  { "let", let_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, let_doc,
     "let arg [arg ...]", (char *)NULL },
  { "read", read_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, read_doc,
     "read [-ers] [-u fd] [-t timeout] [-p prompt] [-a array] [-n nchars] [-d delim] [name ...]", (char *)NULL },
  { "return", return_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, return_doc,
     "return [n]", (char *)NULL },
  { "set", set_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, set_doc,
     "set [--abefhkmnptuvxBCHP] [-o option] [arg ...]", (char *)NULL },
  { "unset", unset_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, unset_doc,
     "unset [-f] [-v] [name ...]", (char *)NULL },
  { "export", export_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN | ASSIGNMENT_BUILTIN, export_doc,
     "export [-nf] [name[=value] ...] or export -p", (char *)NULL },
  { "readonly", readonly_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN | ASSIGNMENT_BUILTIN, readonly_doc,
     "readonly [-af] [name[=value] ...] or readonly -p", (char *)NULL },
  { "shift", shift_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, shift_doc,
     "shift [n]", (char *)NULL },
  { "source", source_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, source_doc,
     "source filename [arguments]", (char *)NULL },
  { ".", source_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, dot_doc,
     ". filename [arguments]", (char *)NULL },
#if defined (JOB_CONTROL)
  { "suspend", suspend_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, suspend_doc,
     "suspend [-f]", (char *)NULL },
#endif /* JOB_CONTROL */
  { "test", test_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, test_doc,
     "test [expr]", (char *)NULL },
  { "[", test_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, test_bracket_doc,
     "[ arg... ]", (char *)NULL },
  { "times", times_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, times_doc,
     "times", (char *)NULL },
  { "trap", trap_builtin, BUILTIN_ENABLED | STATIC_BUILTIN | SPECIAL_BUILTIN, trap_doc,
     "trap [-lp] [arg signal_spec ...]", (char *)NULL },
  { "type", type_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, type_doc,
     "type [-afptP] name [name ...]", (char *)NULL },
#if !defined (_MINIX)
  { "ulimit", ulimit_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, ulimit_doc,
     "ulimit [-SHacdfilmnpqstuvx] [limit]", (char *)NULL },
#endif /* !_MINIX */
  { "umask", umask_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, umask_doc,
     "umask [-p] [-S] [mode]", (char *)NULL },
#if defined (JOB_CONTROL)
  { "wait", wait_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, wait_doc,
     "wait [n]", (char *)NULL },
#endif /* JOB_CONTROL */
#if !defined (JOB_CONTROL)
  { "wait", wait_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, wait_doc,
     "wait [n]", (char *)NULL },
#endif /* !JOB_CONTROL */
  { "for", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, for_doc,
     "for NAME [in WORDS ... ;] do COMMANDS; done", (char *)NULL },
  { "for ((", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, arith_for_doc,
     "for (( exp1; exp2; exp3 )); do COMMANDS; done", (char *)NULL },
  { "select", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, select_doc,
     "select NAME [in WORDS ... ;] do COMMANDS; done", (char *)NULL },
  { "time", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, time_doc,
     "time [-p] PIPELINE", (char *)NULL },
  { "case", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, case_doc,
     "case WORD in [PATTERN [| PATTERN]...) COMMANDS ;;]... esac", (char *)NULL },
  { "if", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, if_doc,
     "if COMMANDS; then COMMANDS; [ elif COMMANDS; then COMMANDS; ]... [ else COMMANDS; ] fi", (char *)NULL },
  { "while", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, while_doc,
     "while COMMANDS; do COMMANDS; done", (char *)NULL },
  { "until", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, until_doc,
     "until COMMANDS; do COMMANDS; done", (char *)NULL },
  { "function", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, function_doc,
     "function NAME { COMMANDS ; } or NAME () { COMMANDS ; }", (char *)NULL },
  { "{ ... }", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, grouping_braces_doc,
     "{ COMMANDS ; }", (char *)NULL },
  { "%", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, fg_percent_doc,
     "JOB_SPEC [&]", (char *)NULL },
  { "(( ... ))", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, arith_doc,
     "(( expression ))", (char *)NULL },
  { "[[ ... ]]", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, conditional_doc,
     "[[ expression ]]", (char *)NULL },
  { "variables", (sh_builtin_func_t *)0x0, BUILTIN_ENABLED | STATIC_BUILTIN, variable_help_doc,
     "variables - Some variable names and meanings", (char *)NULL },
#if defined (PUSHD_AND_POPD)
  { "pushd", pushd_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, pushd_doc,
     "pushd [dir | +N | -N] [-n]", (char *)NULL },
#endif /* PUSHD_AND_POPD */
#if defined (PUSHD_AND_POPD)
  { "popd", popd_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, popd_doc,
     "popd [+N | -N] [-n]", (char *)NULL },
#endif /* PUSHD_AND_POPD */
#if defined (PUSHD_AND_POPD)
  { "dirs", dirs_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, dirs_doc,
     "dirs [-clpv] [+N] [-N]", (char *)NULL },
#endif /* PUSHD_AND_POPD */
  { "shopt", shopt_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, shopt_doc,
     "shopt [-pqsu] [-o long-option] optname [optname...]", (char *)NULL },
  { "printf", printf_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, printf_doc,
     "printf [-v var] format [arguments]", (char *)NULL },
#if defined (PROGRAMMABLE_COMPLETION)
  { "complete", complete_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, complete_doc,
     "complete [-abcdefgjksuv] [-pr] [-o option] [-A action] [-G globpat] [-W wordlist] [-P prefix] [-S suffix] [-X filterpat] [-F function] [-C command] [name ...]", (char *)NULL },
#endif /* PROGRAMMABLE_COMPLETION */
#if defined (PROGRAMMABLE_COMPLETION)
  { "compgen", compgen_builtin, BUILTIN_ENABLED | STATIC_BUILTIN, compgen_doc,
     "compgen [-abcdefgjksuv] [-o option] [-A action] [-G globpat] [-W wordlist] [-P prefix] [-S suffix] [-X filterpat] [-F function] [-C command] [word]", (char *)NULL },
#endif /* PROGRAMMABLE_COMPLETION */
  { (char *)0x0, (sh_builtin_func_t *)0x0, 0, (char **)0x0, (char *)0x0 }
};

struct builtin *shell_builtins = static_shell_builtins;
struct builtin *current_builtin;

int num_shell_builtins =
	sizeof (static_shell_builtins) / sizeof (struct builtin) - 1;
#if defined (ALIAS)
char * const alias_doc[] = {
#if defined (HELP_BUILTIN)
N_("`alias' with no arguments or with the -p option prints the list\n\
    of aliases in the form alias NAME=VALUE on standard output.\n\
    Otherwise, an alias is defined for each NAME whose VALUE is given.\n\
    A trailing space in VALUE causes the next word to be checked for\n\
    alias substitution when the alias is expanded.  Alias returns\n\
    true unless a NAME is given for which no alias has been defined."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* ALIAS */
#if defined (ALIAS)
char * const unalias_doc[] = {
#if defined (HELP_BUILTIN)
N_("Remove NAMEs from the list of defined aliases.  If the -a option is given,\n\
    then remove all alias definitions."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* ALIAS */
#if defined (READLINE)
char * const bind_doc[] = {
#if defined (HELP_BUILTIN)
N_("Bind a key sequence to a Readline function or a macro, or set\n\
    a Readline variable.  The non-option argument syntax is equivalent\n\
    to that found in ~/.inputrc, but must be passed as a single argument:\n\
    bind '\"\\C-x\\C-r\": re-read-init-file'.\n\
    bind accepts the following options:\n\
      -m  keymap         Use `keymap' as the keymap for the duration of this\n\
                         command.  Acceptable keymap names are emacs,\n\
                         emacs-standard, emacs-meta, emacs-ctlx, vi, vi-move,\n\
                         vi-command, and vi-insert.\n\
      -l                 List names of functions.\n\
      -P                 List function names and bindings.\n\
      -p                 List functions and bindings in a form that can be\n\
                         reused as input.\n\
      -r  keyseq         Remove the binding for KEYSEQ.\n\
      -x  keyseq:shell-command	Cause SHELL-COMMAND to be executed when\n\
    				KEYSEQ is entered.\n\
      -f  filename       Read key bindings from FILENAME.\n\
      -q  function-name  Query about which keys invoke the named function.\n\
      -u  function-name  Unbind all keys which are bound to the named function.\n\
      -V                 List variable names and values\n\
      -v                 List variable names and values in a form that can\n\
                         be reused as input.\n\
      -S                 List key sequences that invoke macros and their values\n\
      -s                 List key sequences that invoke macros and their values\n\
                         in a form that can be reused as input."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* READLINE */
char * const break_doc[] = {
#if defined (HELP_BUILTIN)
N_("Exit from within a FOR, WHILE or UNTIL loop.  If N is specified,\n\
    break N levels."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const continue_doc[] = {
#if defined (HELP_BUILTIN)
N_("Resume the next iteration of the enclosing FOR, WHILE or UNTIL loop.\n\
    If N is specified, resume at the N-th enclosing loop."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const builtin_doc[] = {
#if defined (HELP_BUILTIN)
N_("Run a shell builtin.  This is useful when you wish to rename a\n\
    shell builtin to be a function, but need the functionality of the\n\
    builtin within the function itself."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (DEBUGGER)
char * const caller_doc[] = {
#if defined (HELP_BUILTIN)
N_("Returns the context of the current subroutine call.\n\
    \n\
    Without EXPR, returns \"$line $filename\".  With EXPR,\n\
    returns \"$line $subroutine $filename\"; this extra information\n\
    can be used to provide a stack trace.\n\
    \n\
    The value of EXPR indicates how many call frames to go back before the\n\
    current one; the top frame is frame 0."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* DEBUGGER */
char * const cd_doc[] = {
#if defined (HELP_BUILTIN)
N_("Change the current directory to DIR.  The variable $HOME is the\n\
    default DIR.  The variable CDPATH defines the search path for\n\
    the directory containing DIR.  Alternative directory names in CDPATH\n\
    are separated by a colon (:).  A null directory name is the same as\n\
    the current directory, i.e. `.'.  If DIR begins with a slash (/),\n\
    then CDPATH is not used.  If the directory is not found, and the\n\
    shell option `cdable_vars' is set, then try the word as a variable\n\
    name.  If that variable has a value, then cd to the value of that\n\
    variable.  The -P option says to use the physical directory structure\n\
    instead of following symbolic links; the -L option forces symbolic links\n\
    to be followed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const pwd_doc[] = {
#if defined (HELP_BUILTIN)
N_("Print the current working directory.  With the -P option, pwd prints\n\
    the physical directory, without any symbolic links; the -L option\n\
    makes pwd follow symbolic links."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const colon_doc[] = {
#if defined (HELP_BUILTIN)
N_("No effect; the command does nothing.  A zero exit code is returned."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const true_doc[] = {
#if defined (HELP_BUILTIN)
N_("Return a successful result."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const false_doc[] = {
#if defined (HELP_BUILTIN)
N_("Return an unsuccessful result."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const command_doc[] = {
#if defined (HELP_BUILTIN)
N_("Runs COMMAND with ARGS ignoring shell functions.  If you have a shell\n\
    function called `ls', and you wish to call the command `ls', you can\n\
    say \"command ls\".  If the -p option is given, a default value is used\n\
    for PATH that is guaranteed to find all of the standard utilities.  If\n\
    the -V or -v option is given, a string is printed describing COMMAND.\n\
    The -V option produces a more verbose description."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const declare_doc[] = {
#if defined (HELP_BUILTIN)
N_("Declare variables and/or give them attributes.  If no NAMEs are\n\
    given, then display the values of variables instead.  The -p option\n\
    will display the attributes and values of each NAME.\n\
    \n\
    The flags are:\n\
    \n\
      -a	to make NAMEs arrays (if supported)\n\
      -f	to select from among function names only\n\
      -F	to display function names (and line number and source file name if\n\
    	debugging) without definitions\n\
      -i	to make NAMEs have the `integer' attribute\n\
      -r	to make NAMEs readonly\n\
      -t	to make NAMEs have the `trace' attribute\n\
      -x	to make NAMEs export\n\
    \n\
    Variables with the integer attribute have arithmetic evaluation (see\n\
    `let') done when the variable is assigned to.\n\
    \n\
    When displaying values of variables, -f displays a function's name\n\
    and definition.  The -F option restricts the display to function\n\
    name only.\n\
    \n\
    Using `+' instead of `-' turns off the given attribute instead.  When\n\
    used in a function, makes NAMEs local, as with the `local' command."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const typeset_doc[] = {
#if defined (HELP_BUILTIN)
N_("Obsolete.  See `declare'."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const local_doc[] = {
#if defined (HELP_BUILTIN)
N_("Create a local variable called NAME, and give it VALUE.  LOCAL\n\
    can only be used within a function; it makes the variable NAME\n\
    have a visible scope restricted to that function and its children."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (V9_ECHO)
char * const echo_doc[] = {
#if defined (HELP_BUILTIN)
N_("Output the ARGs.  If -n is specified, the trailing newline is\n\
    suppressed.  If the -e option is given, interpretation of the\n\
    following backslash-escaped characters is turned on:\n\
    	\\a	alert (bell)\n\
    	\\b	backspace\n\
    	\\c	suppress trailing newline\n\
    	\\E	escape character\n\
    	\\f	form feed\n\
    	\\n	new line\n\
    	\\r	carriage return\n\
    	\\t	horizontal tab\n\
    	\\v	vertical tab\n\
    	\\\\	backslash\n\
    	\\0nnn	the character whose ASCII code is NNN (octal).  NNN can be\n\
    		0 to 3 octal digits\n\
    \n\
    You can explicitly turn off the interpretation of the above characters\n\
    with the -E option."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* V9_ECHO */
#if !defined (V9_ECHO)
char * const echo_doc[] = {
#if defined (HELP_BUILTIN)
N_("Output the ARGs.  If -n is specified, the trailing newline is suppressed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* !V9_ECHO */
char * const enable_doc[] = {
#if defined (HELP_BUILTIN)
N_("Enable and disable builtin shell commands.  This allows\n\
    you to use a disk command which has the same name as a shell\n\
    builtin without specifying a full pathname.  If -n is used, the\n\
    NAMEs become disabled; otherwise NAMEs are enabled.  For example,\n\
    to use the `test' found in $PATH instead of the shell builtin\n\
    version, type `enable -n test'.  On systems supporting dynamic\n\
    loading, the -f option may be used to load new builtins from the\n\
    shared object FILENAME.  The -d option will delete a builtin\n\
    previously loaded with -f.  If no non-option names are given, or\n\
    the -p option is supplied, a list of builtins is printed.  The\n\
    -a option means to print every builtin with an indication of whether\n\
    or not it is enabled.  The -s option restricts the output to the POSIX.2\n\
    `special' builtins.  The -n option displays a list of all disabled builtins."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const eval_doc[] = {
#if defined (HELP_BUILTIN)
N_("Read ARGs as input to the shell and execute the resulting command(s)."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const getopts_doc[] = {
#if defined (HELP_BUILTIN)
N_("Getopts is used by shell procedures to parse positional parameters.\n\
    \n\
    OPTSTRING contains the option letters to be recognized; if a letter\n\
    is followed by a colon, the option is expected to have an argument,\n\
    which should be separated from it by white space.\n\
    \n\
    Each time it is invoked, getopts will place the next option in the\n\
    shell variable $name, initializing name if it does not exist, and\n\
    the index of the next argument to be processed into the shell\n\
    variable OPTIND.  OPTIND is initialized to 1 each time the shell or\n\
    a shell script is invoked.  When an option requires an argument,\n\
    getopts places that argument into the shell variable OPTARG.\n\
    \n\
    getopts reports errors in one of two ways.  If the first character\n\
    of OPTSTRING is a colon, getopts uses silent error reporting.  In\n\
    this mode, no error messages are printed.  If an invalid option is\n\
    seen, getopts places the option character found into OPTARG.  If a\n\
    required argument is not found, getopts places a ':' into NAME and\n\
    sets OPTARG to the option character found.  If getopts is not in\n\
    silent mode, and an invalid option is seen, getopts places '?' into\n\
    NAME and unsets OPTARG.  If a required argument is not found, a '?'\n\
    is placed in NAME, OPTARG is unset, and a diagnostic message is\n\
    printed.\n\
    \n\
    If the shell variable OPTERR has the value 0, getopts disables the\n\
    printing of error messages, even if the first character of\n\
    OPTSTRING is not a colon.  OPTERR has the value 1 by default.\n\
    \n\
    Getopts normally parses the positional parameters ($0 - $9), but if\n\
    more arguments are given, they are parsed instead."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const exec_doc[] = {
#if defined (HELP_BUILTIN)
N_("Exec FILE, replacing this shell with the specified program.\n\
    If FILE is not specified, the redirections take effect in this\n\
    shell.  If the first argument is `-l', then place a dash in the\n\
    zeroth arg passed to FILE, as login does.  If the `-c' option\n\
    is supplied, FILE is executed with a null environment.  The `-a'\n\
    option means to make set argv[0] of the executed process to NAME.\n\
    If the file cannot be executed and the shell is not interactive,\n\
    then the shell exits, unless the shell option `execfail' is set."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const exit_doc[] = {
#if defined (HELP_BUILTIN)
N_("Exit the shell with a status of N.  If N is omitted, the exit status\n\
    is that of the last command executed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const logout_doc[] = {
#if defined (HELP_BUILTIN)
N_("Logout of a login shell."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (HISTORY)
char * const fc_doc[] = {
#if defined (HELP_BUILTIN)
N_("fc is used to list or edit and re-execute commands from the history list.\n\
    FIRST and LAST can be numbers specifying the range, or FIRST can be a\n\
    string, which means the most recent command beginning with that\n\
    string.\n\
    \n\
       -e ENAME selects which editor to use.  Default is FCEDIT, then EDITOR,\n\
          then vi.\n\
    \n\
       -l means list lines instead of editing.\n\
       -n means no line numbers listed.\n\
       -r means reverse the order of the lines (making it newest listed first).\n\
    \n\
    With the `fc -s [pat=rep ...] [command]' format, the command is\n\
    re-executed after the substitution OLD=NEW is performed.\n\
    \n\
    A useful alias to use with this is r='fc -s', so that typing `r cc'\n\
    runs the last command beginning with `cc' and typing `r' re-executes\n\
    the last command."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* HISTORY */
#if defined (JOB_CONTROL)
char * const fg_doc[] = {
#if defined (HELP_BUILTIN)
N_("Place JOB_SPEC in the foreground, and make it the current job.  If\n\
    JOB_SPEC is not present, the shell's notion of the current job is\n\
    used."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* JOB_CONTROL */
#if defined (JOB_CONTROL)
char * const bg_doc[] = {
#if defined (HELP_BUILTIN)
N_("Place each JOB_SPEC in the background, as if it had been started with\n\
    `&'.  If JOB_SPEC is not present, the shell's notion of the current\n\
    job is used."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* JOB_CONTROL */
char * const hash_doc[] = {
#if defined (HELP_BUILTIN)
N_("For each NAME, the full pathname of the command is determined and\n\
    remembered.  If the -p option is supplied, PATHNAME is used as the\n\
    full pathname of NAME, and no path search is performed.  The -r\n\
    option causes the shell to forget all remembered locations.  The -d\n\
    option causes the shell to forget the remembered location of each NAME.\n\
    If the -t option is supplied the full pathname to which each NAME\n\
    corresponds is printed.  If multiple NAME arguments are supplied with\n\
    -t, the NAME is printed before the hashed full pathname.  The -l option\n\
    causes output to be displayed in a format that may be reused as input.\n\
    If no arguments are given, information about remembered commands is displayed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (HELP_BUILTIN)
char * const help_doc[] = {
#if defined (HELP_BUILTIN)
N_("Display helpful information about builtin commands.  If PATTERN is\n\
    specified, gives detailed help on all commands matching PATTERN,\n\
    otherwise a list of the builtins is printed.  The -s option\n\
    restricts the output for each builtin command matching PATTERN to\n\
    a short usage synopsis."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* HELP_BUILTIN */
#if defined (HISTORY)
char * const history_doc[] = {
#if defined (HELP_BUILTIN)
N_("Display the history list with line numbers.  Lines listed with\n\
    with a `*' have been modified.  Argument of N says to list only\n\
    the last N lines.  The `-c' option causes the history list to be\n\
    cleared by deleting all of the entries.  The `-d' option deletes\n\
    the history entry at offset OFFSET.  The `-w' option writes out the\n\
    current history to the history file;  `-r' means to read the file and\n\
    append the contents to the history list instead.  `-a' means\n\
    to append history lines from this session to the history file.\n\
    Argument `-n' means to read all history lines not already read\n\
    from the history file and append them to the history list.\n\
    \n\
    If FILENAME is given, then that is used as the history file else\n\
    if $HISTFILE has a value, that is used, else ~/.bash_history.\n\
    If the -s option is supplied, the non-option ARGs are appended to\n\
    the history list as a single entry.  The -p option means to perform\n\
    history expansion on each ARG and display the result, without storing\n\
    anything in the history list.\n\
    \n\
    If the $HISTTIMEFORMAT variable is set and not null, its value is used\n\
    as a format string for strftime(3) to print the time stamp associated\n\
    with each displayed history entry.  No time stamps are printed otherwise."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* HISTORY */
#if defined (JOB_CONTROL)
char * const jobs_doc[] = {
#if defined (HELP_BUILTIN)
N_("Lists the active jobs.  The -l option lists process id's in addition\n\
    to the normal information; the -p option lists process id's only.\n\
    If -n is given, only processes that have changed status since the last\n\
    notification are printed.  JOBSPEC restricts output to that job.  The\n\
    -r and -s options restrict output to running and stopped jobs only,\n\
    respectively.  Without options, the status of all active jobs is\n\
    printed.  If -x is given, COMMAND is run after all job specifications\n\
    that appear in ARGS have been replaced with the process ID of that job's\n\
    process group leader."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* JOB_CONTROL */
#if defined (JOB_CONTROL)
char * const disown_doc[] = {
#if defined (HELP_BUILTIN)
N_("By default, removes each JOBSPEC argument from the table of active jobs.\n\
    If the -h option is given, the job is not removed from the table, but is\n\
    marked so that SIGHUP is not sent to the job if the shell receives a\n\
    SIGHUP.  The -a option, when JOBSPEC is not supplied, means to remove all\n\
    jobs from the job table; the -r option means to remove only running jobs."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* JOB_CONTROL */
char * const kill_doc[] = {
#if defined (HELP_BUILTIN)
N_("Send the processes named by PID (or JOBSPEC) the signal SIGSPEC.  If\n\
    SIGSPEC is not present, then SIGTERM is assumed.  An argument of `-l'\n\
    lists the signal names; if arguments follow `-l' they are assumed to\n\
    be signal numbers for which names should be listed.  Kill is a shell\n\
    builtin for two reasons: it allows job IDs to be used instead of\n\
    process IDs, and, if you have reached the limit on processes that\n\
    you can create, you don't have to start a process to kill another one."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const let_doc[] = {
#if defined (HELP_BUILTIN)
N_("Each ARG is an arithmetic expression to be evaluated.  Evaluation\n\
    is done in fixed-width integers with no check for overflow, though\n\
    division by 0 is trapped and flagged as an error.  The following\n\
    list of operators is grouped into levels of equal-precedence operators.\n\
    The levels are listed in order of decreasing precedence.\n\
    \n\
    	id++, id--	variable post-increment, post-decrement\n\
    	++id, --id	variable pre-increment, pre-decrement\n\
    	-, +		unary minus, plus\n\
    	!, ~		logical and bitwise negation\n\
    	**		exponentiation\n\
    	*, /, %		multiplication, division, remainder\n\
    	+, -		addition, subtraction\n\
    	<<, >>		left and right bitwise shifts\n\
    	<=, >=, <, >	comparison\n\
    	==, !=		equality, inequality\n\
    	&		bitwise AND\n\
    	^		bitwise XOR\n\
    	|		bitwise OR\n\
    	&&		logical AND\n\
    	||		logical OR\n\
    	expr ? expr : expr\n\
    			conditional operator\n\
    	=, *=, /=, %=,\n\
    	+=, -=, <<=, >>=,\n\
    	&=, ^=, |=	assignment\n\
    \n\
    Shell variables are allowed as operands.  The name of the variable\n\
    is replaced by its value (coerced to a fixed-width integer) within\n\
    an expression.  The variable need not have its integer attribute\n\
    turned on to be used in an expression.\n\
    \n\
    Operators are evaluated in order of precedence.  Sub-expressions in\n\
    parentheses are evaluated first and may override the precedence\n\
    rules above.\n\
    \n\
    If the last ARG evaluates to 0, let returns 1; 0 is returned\n\
    otherwise."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const read_doc[] = {
#if defined (HELP_BUILTIN)
N_("One line is read from the standard input, or from file descriptor FD if the\n\
    -u option is supplied, and the first word is assigned to the first NAME,\n\
    the second word to the second NAME, and so on, with leftover words assigned\n\
    to the last NAME.  Only the characters found in $IFS are recognized as word\n\
    delimiters.  If no NAMEs are supplied, the line read is stored in the REPLY\n\
    variable.  If the -r option is given, this signifies `raw' input, and\n\
    backslash escaping is disabled.  The -d option causes read to continue\n\
    until the first character of DELIM is read, rather than newline.  If the -p\n\
    option is supplied, the string PROMPT is output without a trailing newline\n\
    before attempting to read.  If -a is supplied, the words read are assigned\n\
    to sequential indices of ARRAY, starting at zero.  If -e is supplied and\n\
    the shell is interactive, readline is used to obtain the line.  If -n is\n\
    supplied with a non-zero NCHARS argument, read returns after NCHARS\n\
    characters have been read.  The -s option causes input coming from a\n\
    terminal to not be echoed.\n\
    \n\
    The -t option causes read to time out and return failure if a complete line\n\
    of input is not read within TIMEOUT seconds.  If the TMOUT variable is set,\n\
    its value is the default timeout.  The return code is zero, unless end-of-file\n\
    is encountered, read times out, or an invalid file descriptor is supplied as\n\
    the argument to -u."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const return_doc[] = {
#if defined (HELP_BUILTIN)
N_("Causes a function to exit with the return value specified by N.  If N\n\
    is omitted, the return status is that of the last command."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const set_doc[] = {
#if defined (HELP_BUILTIN)
N_("    -a  Mark variables which are modified or created for export.\n\
        -b  Notify of job termination immediately.\n\
        -e  Exit immediately if a command exits with a non-zero status.\n\
        -f  Disable file name generation (globbing).\n\
        -h  Remember the location of commands as they are looked up.\n\
        -k  All assignment arguments are placed in the environment for a\n\
            command, not just those that precede the command name.\n\
        -m  Job control is enabled.\n\
        -n  Read commands but do not execute them.\n\
        -o option-name\n\
            Set the variable corresponding to option-name:\n\
                allexport    same as -a\n\
                braceexpand  same as -B\n\
                emacs        use an emacs-style line editing interface\n\
                errexit      same as -e\n\
                errtrace     same as -E\n\
                functrace    same as -T\n\
                hashall      same as -h\n\
                histexpand   same as -H\n\
                history      enable command history\n\
                ignoreeof    the shell will not exit upon reading EOF\n\
                interactive-comments\n\
                             allow comments to appear in interactive commands\n\
                keyword      same as -k\n\
                monitor      same as -m\n\
                noclobber    same as -C\n\
                noexec       same as -n\n\
                noglob       same as -f\n\
                nolog        currently accepted but ignored\n\
                notify       same as -b\n\
                nounset      same as -u\n\
                onecmd       same as -t\n\
                physical     same as -P\n\
                pipefail     the return value of a pipeline is the status of\n\
                             the last command to exit with a non-zero status,\n\
                             or zero if no command exited with a non-zero status\n\
                posix        change the behavior of bash where the default\n\
                             operation differs from the 1003.2 standard to\n\
                             match the standard\n\
                privileged   same as -p\n\
                verbose      same as -v\n\
                vi           use a vi-style line editing interface\n\
                xtrace       same as -x\n\
        -p  Turned on whenever the real and effective user ids do not match.\n\
            Disables processing of the $ENV file and importing of shell\n\
            functions.  Turning this option off causes the effective uid and\n\
            gid to be set to the real uid and gid.\n\
        -t  Exit after reading and executing one command.\n\
        -u  Treat unset variables as an error when substituting.\n\
        -v  Print shell input lines as they are read.\n\
        -x  Print commands and their arguments as they are executed.\n\
        -B  the shell will perform brace expansion\n\
        -C  If set, disallow existing regular files to be overwritten\n\
            by redirection of output.\n\
        -E  If set, the ERR trap is inherited by shell functions.\n\
        -H  Enable ! style history substitution.  This flag is on\n\
            by default when the shell is interactive.\n\
        -P  If set, do not follow symbolic links when executing commands\n\
            such as cd which change the current directory.\n\
        -T  If set, the DEBUG trap is inherited by shell functions.\n\
        -   Assign any remaining arguments to the positional parameters.\n\
            The -x and -v options are turned off.\n\
    \n\
    Using + rather than - causes these flags to be turned off.  The\n\
    flags can also be used upon invocation of the shell.  The current\n\
    set of flags may be found in $-.  The remaining n ARGs are positional\n\
    parameters and are assigned, in order, to $1, $2, .. $n.  If no\n\
    ARGs are given, all shell variables are printed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const unset_doc[] = {
#if defined (HELP_BUILTIN)
N_("For each NAME, remove the corresponding variable or function.  Given\n\
    the `-v', unset will only act on variables.  Given the `-f' flag,\n\
    unset will only act on functions.  With neither flag, unset first\n\
    tries to unset a variable, and if that fails, then tries to unset a\n\
    function.  Some variables cannot be unset; also see readonly."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const export_doc[] = {
#if defined (HELP_BUILTIN)
N_("NAMEs are marked for automatic export to the environment of\n\
    subsequently executed commands.  If the -f option is given,\n\
    the NAMEs refer to functions.  If no NAMEs are given, or if `-p'\n\
    is given, a list of all names that are exported in this shell is\n\
    printed.  An argument of `-n' says to remove the export property\n\
    from subsequent NAMEs.  An argument of `--' disables further option\n\
    processing."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const readonly_doc[] = {
#if defined (HELP_BUILTIN)
N_("The given NAMEs are marked readonly and the values of these NAMEs may\n\
    not be changed by subsequent assignment.  If the -f option is given,\n\
    then functions corresponding to the NAMEs are so marked.  If no\n\
    arguments are given, or if `-p' is given, a list of all readonly names\n\
    is printed.  The `-a' option means to treat each NAME as\n\
    an array variable.  An argument of `--' disables further option\n\
    processing."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const shift_doc[] = {
#if defined (HELP_BUILTIN)
N_("The positional parameters from $N+1 ... are renamed to $1 ...  If N is\n\
    not given, it is assumed to be 1."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const source_doc[] = {
#if defined (HELP_BUILTIN)
N_("Read and execute commands from FILENAME and return.  The pathnames\n\
    in $PATH are used to find the directory containing FILENAME.  If any\n\
    ARGUMENTS are supplied, they become the positional parameters when\n\
    FILENAME is executed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const dot_doc[] = {
#if defined (HELP_BUILTIN)
N_("Read and execute commands from FILENAME and return.  The pathnames\n\
    in $PATH are used to find the directory containing FILENAME.  If any\n\
    ARGUMENTS are supplied, they become the positional parameters when\n\
    FILENAME is executed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (JOB_CONTROL)
char * const suspend_doc[] = {
#if defined (HELP_BUILTIN)
N_("Suspend the execution of this shell until it receives a SIGCONT\n\
    signal.  The `-f' if specified says not to complain about this\n\
    being a login shell if it is; just suspend anyway."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* JOB_CONTROL */
char * const test_doc[] = {
#if defined (HELP_BUILTIN)
N_("Exits with a status of 0 (true) or 1 (false) depending on\n\
    the evaluation of EXPR.  Expressions may be unary or binary.  Unary\n\
    expressions are often used to examine the status of a file.  There\n\
    are string operators as well, and numeric comparison operators.\n\
    \n\
    File operators:\n\
    \n\
        -a FILE        True if file exists.\n\
        -b FILE        True if file is block special.\n\
        -c FILE        True if file is character special.\n\
        -d FILE        True if file is a directory.\n\
        -e FILE        True if file exists.\n\
        -f FILE        True if file exists and is a regular file.\n\
        -g FILE        True if file is set-group-id.\n\
        -h FILE        True if file is a symbolic link.\n\
        -L FILE        True if file is a symbolic link.\n\
        -k FILE        True if file has its `sticky' bit set.\n\
        -p FILE        True if file is a named pipe.\n\
        -r FILE        True if file is readable by you.\n\
        -s FILE        True if file exists and is not empty.\n\
        -S FILE        True if file is a socket.\n\
        -t FD          True if FD is opened on a terminal.\n\
        -u FILE        True if the file is set-user-id.\n\
        -w FILE        True if the file is writable by you.\n\
        -x FILE        True if the file is executable by you.\n\
        -O FILE        True if the file is effectively owned by you.\n\
        -G FILE        True if the file is effectively owned by your group.\n\
        -N FILE        True if the file has been modified since it was last read.\n\
    \n\
      FILE1 -nt FILE2  True if file1 is newer than file2 (according to\n\
                       modification date).\n\
    \n\
      FILE1 -ot FILE2  True if file1 is older than file2.\n\
    \n\
      FILE1 -ef FILE2  True if file1 is a hard link to file2.\n\
    \n\
    String operators:\n\
    \n\
        -z STRING      True if string is empty.\n\
    \n\
        -n STRING\n\
        STRING         True if string is not empty.\n\
    \n\
        STRING1 = STRING2\n\
                       True if the strings are equal.\n\
        STRING1 != STRING2\n\
                       True if the strings are not equal.\n\
        STRING1 < STRING2\n\
                       True if STRING1 sorts before STRING2 lexicographically.\n\
        STRING1 > STRING2\n\
                       True if STRING1 sorts after STRING2 lexicographically.\n\
    \n\
    Other operators:\n\
    \n\
        -o OPTION      True if the shell option OPTION is enabled.\n\
        ! EXPR         True if expr is false.\n\
        EXPR1 -a EXPR2 True if both expr1 AND expr2 are true.\n\
        EXPR1 -o EXPR2 True if either expr1 OR expr2 is true.\n\
    \n\
        arg1 OP arg2   Arithmetic tests.  OP is one of -eq, -ne,\n\
                       -lt, -le, -gt, or -ge.\n\
    \n\
    Arithmetic binary operators return true if ARG1 is equal, not-equal,\n\
    less-than, less-than-or-equal, greater-than, or greater-than-or-equal\n\
    than ARG2."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const test_bracket_doc[] = {
#if defined (HELP_BUILTIN)
N_("This is a synonym for the \"test\" builtin, but the last\n\
    argument must be a literal `]', to match the opening `['."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const times_doc[] = {
#if defined (HELP_BUILTIN)
N_("Print the accumulated user and system times for processes run from\n\
    the shell."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const trap_doc[] = {
#if defined (HELP_BUILTIN)
N_("The command ARG is to be read and executed when the shell receives\n\
    signal(s) SIGNAL_SPEC.  If ARG is absent (and a single SIGNAL_SPEC\n\
    is supplied) or `-', each specified signal is reset to its original\n\
    value.  If ARG is the null string each SIGNAL_SPEC is ignored by the\n\
    shell and by the commands it invokes.  If a SIGNAL_SPEC is EXIT (0)\n\
    the command ARG is executed on exit from the shell.  If a SIGNAL_SPEC\n\
    is DEBUG, ARG is executed after every simple command.  If the`-p' option\n\
    is supplied then the trap commands associated with each SIGNAL_SPEC are\n\
    displayed.  If no arguments are supplied or if only `-p' is given, trap\n\
    prints the list of commands associated with each signal.  Each SIGNAL_SPEC\n\
    is either a signal name in <signal.h> or a signal number.  Signal names\n\
    are case insensitive and the SIG prefix is optional.  `trap -l' prints\n\
    a list of signal names and their corresponding numbers.  Note that a\n\
    signal can be sent to the shell with \"kill -signal $$\"."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const type_doc[] = {
#if defined (HELP_BUILTIN)
N_("For each NAME, indicate how it would be interpreted if used as a\n\
    command name.\n\
    \n\
    If the -t option is used, `type' outputs a single word which is one of\n\
    `alias', `keyword', `function', `builtin', `file' or `', if NAME is an\n\
    alias, shell reserved word, shell function, shell builtin, disk file,\n\
    or unfound, respectively.\n\
    \n\
    If the -p flag is used, `type' either returns the name of the disk\n\
    file that would be executed, or nothing if `type -t NAME' would not\n\
    return `file'.\n\
    \n\
    If the -a flag is used, `type' displays all of the places that contain\n\
    an executable named `file'.  This includes aliases, builtins, and\n\
    functions, if and only if the -p flag is not also used.\n\
    \n\
    The -f flag suppresses shell function lookup.\n\
    \n\
    The -P flag forces a PATH search for each NAME, even if it is an alias,\n\
    builtin, or function, and returns the name of the disk file that would\n\
    be executed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if !defined (_MINIX)
char * const ulimit_doc[] = {
#if defined (HELP_BUILTIN)
N_("Ulimit provides control over the resources available to processes\n\
    started by the shell, on systems that allow such control.  If an\n\
    option is given, it is interpreted as follows:\n\
    \n\
        -S	use the `soft' resource limit\n\
        -H	use the `hard' resource limit\n\
        -a	all current limits are reported\n\
        -c	the maximum size of core files created\n\
        -d	the maximum size of a process's data segment\n\
        -e	the maximum scheduling priority (`nice')\n\
        -f	the maximum size of files written by the shell and its children\n\
        -i	the maximum number of pending signals\n\
        -l	the maximum size a process may lock into memory\n\
        -m	the maximum resident set size\n\
        -n	the maximum number of open file descriptors\n\
        -p	the pipe buffer size\n\
        -q	the maximum number of bytes in POSIX message queues\n\
        -r	the maximum real-time scheduling priority\n\
        -s	the maximum stack size\n\
        -t	the maximum amount of cpu time in seconds\n\
        -u	the maximum number of user processes\n\
        -v	the size of virtual memory\n\
        -x	the maximum number of file locks\n\
    \n\
    If LIMIT is given, it is the new value of the specified resource;\n\
    the special LIMIT values `soft', `hard', and `unlimited' stand for\n\
    the current soft limit, the current hard limit, and no limit, respectively.\n\
    Otherwise, the current value of the specified resource is printed.\n\
    If no option is given, then -f is assumed.  Values are in 1024-byte\n\
    increments, except for -t, which is in seconds, -p, which is in\n\
    increments of 512 bytes, and -u, which is an unscaled number of\n\
    processes."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* !_MINIX */
char * const umask_doc[] = {
#if defined (HELP_BUILTIN)
N_("The user file-creation mask is set to MODE.  If MODE is omitted, or if\n\
    `-S' is supplied, the current value of the mask is printed.  The `-S'\n\
    option makes the output symbolic; otherwise an octal number is output.\n\
    If `-p' is supplied, and MODE is omitted, the output is in a form\n\
    that may be used as input.  If MODE begins with a digit, it is\n\
    interpreted as an octal number, otherwise it is a symbolic mode string\n\
    like that accepted by chmod(1)."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (JOB_CONTROL)
char * const wait_doc[] = {
#if defined (HELP_BUILTIN)
N_("Wait for the specified process and report its termination status.  If\n\
    N is not given, all currently active child processes are waited for,\n\
    and the return code is zero.  N may be a process ID or a job\n\
    specification; if a job spec is given, all processes in the job's\n\
    pipeline are waited for."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* JOB_CONTROL */
#if !defined (JOB_CONTROL)
char * const wait_doc[] = {
#if defined (HELP_BUILTIN)
N_("Wait for the specified process and report its termination status.  If\n\
    N is not given, all currently active child processes are waited for,\n\
    and the return code is zero.  N is a process ID; if it is not given,\n\
    all child processes of the shell are waited for."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* !JOB_CONTROL */
char * const for_doc[] = {
#if defined (HELP_BUILTIN)
N_("The `for' loop executes a sequence of commands for each member in a\n\
    list of items.  If `in WORDS ...;' is not present, then `in \"$@\"' is\n\
    assumed.  For each element in WORDS, NAME is set to that element, and\n\
    the COMMANDS are executed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const arith_for_doc[] = {
#if defined (HELP_BUILTIN)
N_("Equivalent to\n\
    	(( EXP1 ))\n\
    	while (( EXP2 )); do\n\
    		COMMANDS\n\
    		(( EXP3 ))\n\
    	done\n\
    EXP1, EXP2, and EXP3 are arithmetic expressions.  If any expression is\n\
    omitted, it behaves as if it evaluates to 1."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const select_doc[] = {
#if defined (HELP_BUILTIN)
N_("The WORDS are expanded, generating a list of words.  The\n\
    set of expanded words is printed on the standard error, each\n\
    preceded by a number.  If `in WORDS' is not present, `in \"$@\"'\n\
    is assumed.  The PS3 prompt is then displayed and a line read\n\
    from the standard input.  If the line consists of the number\n\
    corresponding to one of the displayed words, then NAME is set\n\
    to that word.  If the line is empty, WORDS and the prompt are\n\
    redisplayed.  If EOF is read, the command completes.  Any other\n\
    value read causes NAME to be set to null.  The line read is saved\n\
    in the variable REPLY.  COMMANDS are executed after each selection\n\
    until a break command is executed."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const time_doc[] = {
#if defined (HELP_BUILTIN)
N_("Execute PIPELINE and print a summary of the real time, user CPU time,\n\
    and system CPU time spent executing PIPELINE when it terminates.\n\
    The return status is the return status of PIPELINE.  The `-p' option\n\
    prints the timing summary in a slightly different format.  This uses\n\
    the value of the TIMEFORMAT variable as the output format."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const case_doc[] = {
#if defined (HELP_BUILTIN)
N_("Selectively execute COMMANDS based upon WORD matching PATTERN.  The\n\
    `|' is used to separate multiple patterns."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const if_doc[] = {
#if defined (HELP_BUILTIN)
N_("The `if COMMANDS' list is executed.  If its exit status is zero, then the\n\
    `then COMMANDS' list is executed.  Otherwise, each `elif COMMANDS' list is\n\
    executed in turn, and if its exit status is zero, the corresponding\n\
    `then COMMANDS' list is executed and the if command completes.  Otherwise,\n\
    the `else COMMANDS' list is executed, if present.  The exit status of the\n\
    entire construct is the exit status of the last command executed, or zero\n\
    if no condition tested true."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const while_doc[] = {
#if defined (HELP_BUILTIN)
N_("Expand and execute COMMANDS as long as the final command in the\n\
    `while' COMMANDS has an exit status of zero."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const until_doc[] = {
#if defined (HELP_BUILTIN)
N_("Expand and execute COMMANDS as long as the final command in the\n\
    `until' COMMANDS has an exit status which is not zero."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const function_doc[] = {
#if defined (HELP_BUILTIN)
N_("Create a simple command invoked by NAME which runs COMMANDS.\n\
    Arguments on the command line along with NAME are passed to the\n\
    function as $0 .. $n."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const grouping_braces_doc[] = {
#if defined (HELP_BUILTIN)
N_("Run a set of commands in a group.  This is one way to redirect an\n\
    entire set of commands."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const fg_percent_doc[] = {
#if defined (HELP_BUILTIN)
N_("Equivalent to the JOB_SPEC argument to the `fg' command.  Resume a\n\
    stopped or background job.  JOB_SPEC can specify either a job name\n\
    or a job number.  Following JOB_SPEC with a `&' places the job in\n\
    the background, as if the job specification had been supplied as an\n\
    argument to `bg'."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const arith_doc[] = {
#if defined (HELP_BUILTIN)
N_("The EXPRESSION is evaluated according to the rules for arithmetic\n\
    evaluation.  Equivalent to \"let EXPRESSION\"."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const conditional_doc[] = {
#if defined (HELP_BUILTIN)
N_("Returns a status of 0 or 1 depending on the evaluation of the conditional\n\
    expression EXPRESSION.  Expressions are composed of the same primaries used\n\
    by the `test' builtin, and may be combined using the following operators\n\
    \n\
    	( EXPRESSION )	Returns the value of EXPRESSION\n\
    	! EXPRESSION	True if EXPRESSION is false; else false\n\
    	EXPR1 && EXPR2	True if both EXPR1 and EXPR2 are true; else false\n\
    	EXPR1 || EXPR2	True if either EXPR1 or EXPR2 is true; else false\n\
    \n\
    When the `==' and `!=' operators are used, the string to the right of the\n\
    operator is used as a pattern and pattern matching is performed.  The\n\
    && and || operators do not evaluate EXPR2 if EXPR1 is sufficient to\n\
    determine the expression's value."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const variable_help_doc[] = {
#if defined (HELP_BUILTIN)
N_("BASH_VERSION	Version information for this Bash.\n\
    CDPATH	A colon-separated list of directories to search\n\
    		for directries given as arguments to `cd'.\n\
    GLOBIGNORE	A colon-separated list of patterns describing filenames to\n\
    		be ignored by pathname expansion.\n\
    HISTFILE	The name of the file where your command history is stored.\n\
    HISTFILESIZE	The maximum number of lines this file can contain.\n\
    HISTSIZE	The maximum number of history lines that a running\n\
    		shell can access.\n\
    HOME	The complete pathname to your login directory.\n\
    HOSTNAME	The name of the current host.\n\
    HOSTTYPE	The type of CPU this version of Bash is running under.\n\
    IGNOREEOF	Controls the action of the shell on receipt of an EOF\n\
    		character as the sole input.  If set, then the value\n\
    		of it is the number of EOF characters that can be seen\n\
    		in a row on an empty line before the shell will exit\n\
    		(default 10).  When unset, EOF signifies the end of input.\n\
    MACHTYPE	A string describing the current system Bash is running on.\n\
    MAILCHECK	How often, in seconds, Bash checks for new mail.\n\
    MAILPATH	A colon-separated list of filenames which Bash checks\n\
    		for new mail.\n\
    OSTYPE	The version of Unix this version of Bash is running on.\n\
    PATH	A colon-separated list of directories to search when\n\
    		looking for commands.\n\
    PROMPT_COMMAND	A command to be executed before the printing of each\n\
    		primary prompt.\n\
    PS1		The primary prompt string.\n\
    PS2		The secondary prompt string.\n\
    PWD		The full pathname of the current directory.\n\
    SHELLOPTS	A colon-separated list of enabled shell options.\n\
    TERM	The name of the current terminal type.\n\
    TIMEFORMAT	The output format for timing statistics displayed by the\n\
    		`time' reserved word.\n\
    auto_resume	Non-null means a command word appearing on a line by\n\
    		itself is first looked for in the list of currently\n\
    		stopped jobs.  If found there, that job is foregrounded.\n\
    		A value of `exact' means that the command word must\n\
    		exactly match a command in the list of stopped jobs.  A\n\
    		value of `substring' means that the command word must\n\
    		match a substring of the job.  Any other value means that\n\
    		the command must be a prefix of a stopped job.\n\
    histchars	Characters controlling history expansion and quick\n\
    		substitution.  The first character is the history\n\
    		substitution character, usually `!'.  The second is\n\
    		the `quick substitution' character, usually `^'.  The\n\
    		third is the `history comment' character, usually `#'.\n\
    HISTIGNORE	A colon-separated list of patterns used to decide which\n\
    		commands should be saved on the history list.\n\
"),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (PUSHD_AND_POPD)
char * const pushd_doc[] = {
#if defined (HELP_BUILTIN)
N_("Adds a directory to the top of the directory stack, or rotates\n\
    the stack, making the new top of the stack the current working\n\
    directory.  With no arguments, exchanges the top two directories.\n\
    \n\
    +N	Rotates the stack so that the Nth directory (counting\n\
    	from the left of the list shown by `dirs', starting with\n\
    	zero) is at the top.\n\
    \n\
    -N	Rotates the stack so that the Nth directory (counting\n\
    	from the right of the list shown by `dirs', starting with\n\
    	zero) is at the top.\n\
    \n\
    -n	suppress the normal change of directory when adding directories\n\
    	to the stack, so only the stack is manipulated.\n\
    \n\
    dir	adds DIR to the directory stack at the top, making it the\n\
    	new current working directory.\n\
    \n\
    You can see the directory stack with the `dirs' command."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* PUSHD_AND_POPD */
#if defined (PUSHD_AND_POPD)
char * const popd_doc[] = {
#if defined (HELP_BUILTIN)
N_("Removes entries from the directory stack.  With no arguments,\n\
    removes the top directory from the stack, and cd's to the new\n\
    top directory.\n\
    \n\
    +N	removes the Nth entry counting from the left of the list\n\
    	shown by `dirs', starting with zero.  For example: `popd +0'\n\
    	removes the first directory, `popd +1' the second.\n\
    \n\
    -N	removes the Nth entry counting from the right of the list\n\
    	shown by `dirs', starting with zero.  For example: `popd -0'\n\
    	removes the last directory, `popd -1' the next to last.\n\
    \n\
    -n	suppress the normal change of directory when removing directories\n\
    	from the stack, so only the stack is manipulated.\n\
    \n\
    You can see the directory stack with the `dirs' command."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* PUSHD_AND_POPD */
#if defined (PUSHD_AND_POPD)
char * const dirs_doc[] = {
#if defined (HELP_BUILTIN)
N_("Display the list of currently remembered directories.  Directories\n\
    find their way onto the list with the `pushd' command; you can get\n\
    back up through the list with the `popd' command.\n\
    \n\
    The -l flag specifies that `dirs' should not print shorthand versions\n\
    of directories which are relative to your home directory.  This means\n\
    that `~/bin' might be displayed as `/homes/bfox/bin'.  The -v flag\n\
    causes `dirs' to print the directory stack with one entry per line,\n\
    prepending the directory name with its position in the stack.  The -p\n\
    flag does the same thing, but the stack position is not prepended.\n\
    The -c flag clears the directory stack by deleting all of the elements.\n\
    \n\
    +N	displays the Nth entry counting from the left of the list shown by\n\
    	dirs when invoked without options, starting with zero.\n\
    \n\
    -N	displays the Nth entry counting from the right of the list shown by\n\
    	dirs when invoked without options, starting with zero."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* PUSHD_AND_POPD */
char * const shopt_doc[] = {
#if defined (HELP_BUILTIN)
N_("Toggle the values of variables controlling optional behavior.\n\
    The -s flag means to enable (set) each OPTNAME; the -u flag\n\
    unsets each OPTNAME.  The -q flag suppresses output; the exit\n\
    status indicates whether each OPTNAME is set or unset.  The -o\n\
    option restricts the OPTNAMEs to those defined for use with\n\
    `set -o'.  With no options, or with the -p option, a list of all\n\
    settable options is displayed, with an indication of whether or\n\
    not each is set."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
char * const printf_doc[] = {
#if defined (HELP_BUILTIN)
N_("printf formats and prints ARGUMENTS under control of the FORMAT. FORMAT\n\
    is a character string which contains three types of objects: plain\n\
    characters, which are simply copied to standard output, character escape\n\
    sequences which are converted and copied to the standard output, and\n\
    format specifications, each of which causes printing of the next successive\n\
    argument.  In addition to the standard printf(1) formats, %b means to\n\
    expand backslash escape sequences in the corresponding argument, and %q\n\
    means to quote the argument in a way that can be reused as shell input.\n\
    If the -v option is supplied, the output is placed into the value of the\n\
    shell variable VAR rather than being sent to the standard output."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#if defined (PROGRAMMABLE_COMPLETION)
char * const complete_doc[] = {
#if defined (HELP_BUILTIN)
N_("For each NAME, specify how arguments are to be completed.\n\
    If the -p option is supplied, or if no options are supplied, existing\n\
    completion specifications are printed in a way that allows them to be\n\
    reused as input.  The -r option removes a completion specification for\n\
    each NAME, or, if no NAMEs are supplied, all completion specifications."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* PROGRAMMABLE_COMPLETION */
#if defined (PROGRAMMABLE_COMPLETION)
char * const compgen_doc[] = {
#if defined (HELP_BUILTIN)
N_("Display the possible completions depending on the options.  Intended\n\
    to be used from within a shell function generating possible completions.\n\
    If the optional WORD argument is supplied, matches against WORD are\n\
    generated."),
#endif /* HELP_BUILTIN */
  (char *)NULL
};
#endif /* PROGRAMMABLE_COMPLETION */
