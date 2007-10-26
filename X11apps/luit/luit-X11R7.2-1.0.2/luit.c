/*
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* $XFree86: xc/programs/luit/luit.c,v 1.9 2002/10/17 01:06:09 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <signal.h>

#ifdef SVR4
#include <stropts.h>
#endif

#include <X11/fonts/fontenc.h>
#include "luit.h"
#include "sys.h"
#include "other.h"
#include "charset.h"
#include "iso2022.h"

static Iso2022Ptr inputState = NULL, outputState = NULL;

static char *child_argv0 = NULL;
static char *locale_name = NULL;
int ilog = -1;
int olog = -1;
int verbose = 0;
int converter = 0;
int exitOnChild = 0;

volatile int sigwinch_queued = 0;
volatile int sigchld_queued = 0;

static int convert(int, int);
static int condom(int, char**);

static void
ErrorF(char *f, ...)
{
    va_list args;
    va_start(args, f);
    vfprintf(stderr, f, args);
    va_end(args);
}

static void
FatalError(char *f, ...)
{
    va_list args;
    va_start(args, f);
    vfprintf(stderr, f, args);
    va_end(args);
    exit(1);
}


static void
help(void)
{
    fprintf(stderr, 
            "luit\n"
            "  [ -h ] [ -list ] [ -v ] [ -argv0 name ]\n"
            "  [ -gl gn ] [-gr gk] "
            "[ -g0 set ] [ -g1 set ] "
            "[ -g2 set ] [ -g3 set ]\n"
            "  [ -encoding encoding ] "
            "[ +oss ] [ +ols ] [ +osl ] [ +ot ]\n"
            "  [ -kgl gn ] [-kgr gk] "
            "[ -kg0 set ] [ -kg1 set ] "
            "[ -kg2 set ] [ -kg3 set ]\n"
            "  [ -k7 ] [ +kss ] [ +kssgr ] [ -kls ]\n"
            "  [ -c ] [ -x ] [ -ilog filename ] [ -olog filename ] [ -- ]\n"
            "  [ program [ args ] ]\n");

}
            

static int
parseOptions(int argc, char **argv)
{
    int i = 1;
    while(i < argc) {
        if(argv[i][0] != '-' && argv[i][0] != '+') {
            break;
        } else if(!strcmp(argv[i], "--")) {
            i++;
            break;
        } else if(!strcmp(argv[i], "-v")) {
            verbose++;
            i++;
        } else if(!strcmp(argv[i], "-h")) {
            help();
            exit(0);
        } else if(!strcmp(argv[i], "-list")) {
            reportCharsets();
            exit(0);
        } else if(!strcmp(argv[i], "+oss")) {
            outputState->outputFlags &= ~OF_SS;
            i++;
        } else if(!strcmp(argv[i], "+ols")) {
            outputState->outputFlags &= ~OF_LS;
            i++;
        } else if(!strcmp(argv[i], "+osl")) {
            outputState->outputFlags &= ~OF_SELECT;
            i++;
        } else if(!strcmp(argv[i], "+ot")) {
            outputState->outputFlags = OF_PASSTHRU;
            i++;
        } else if(!strcmp(argv[i], "-k7")) {
            inputState->inputFlags &= ~IF_EIGHTBIT;
            i++;
        } else if(!strcmp(argv[i], "+kss")) {
            inputState->inputFlags &= ~IF_SS;
            i++;
        } else if(!strcmp(argv[1], "+kssgr")) {
            inputState->inputFlags &= ~IF_SSGR;
            i++;
        } else if(!strcmp(argv[i], "-kls")) {
            inputState->inputFlags |= IF_LS;
            i++;
        } else if(!strcmp(argv[i], "-g0")) {
            if(i + 1 >= argc)
                FatalError("-g0 requires an argument\n");
            G0(outputState) = getCharsetByName(argv[i + 1]);
            i += 2;
        } else if(!strcmp(argv[i], "-g1")) {
            if(i + 1 >= argc)
                FatalError("-g1 requires an argument\n");
            G1(outputState) = getCharsetByName(argv[i + 1]);
            i += 2;
        } else if(!strcmp(argv[i], "-g2")) {
            if(i + 1 >= argc)
                FatalError("-g2 requires an argument\n");
            G2(outputState) = getCharsetByName(argv[i + 1]);
            i += 2;
        } else if(!strcmp(argv[i], "-g3")) {
            if(i + 1 >= argc)
                FatalError("-g3 requires an argument\n");
            G3(outputState) = getCharsetByName(argv[i + 1]);

            i += 2;
        } else if(!strcmp(argv[i], "-gl")) {
            int j;
            if(i + 1 >= argc)
                FatalError("-gl requires an argument\n");
            if(strlen(argv[i + 1]) != 2 ||
               argv[i + 1][0] != 'g')
                j = -1;
            else 
                j = argv[i + 1][1] - '0';
            if(j < 0 || j > 3)
                FatalError("The argument of -gl "
                           "should be one of g0 through g3,\n"
                           "not %s\n", argv[i + 1]);
            else
                outputState->glp = &outputState->g[j];
            i += 2;
        } else if(!strcmp(argv[i], "-gr")) {
            int j;
            if(i + 1 >= argc)
                FatalError("-gr requires an argument\n");
            if(strlen(argv[i + 1]) != 2 ||
               argv[i + 1][0] != 'g')
                j = -1;
            else 
                j = argv[i + 1][1] - '0';
            if(j < 0 || j > 3)
                FatalError("The argument of -gl "
                           "should be one of g0 through g3,\n"
                           "not %s\n", argv[i + 1]);
            else
                outputState->grp = &outputState->g[j];
            i += 2;
        } else if(!strcmp(argv[i], "-kg0")) {
            if(i + 1 >= argc)
                FatalError("-kg0 requires an argument\n");
            G0(inputState) = getCharsetByName(argv[i + 1]);
            i += 2;
        } else if(!strcmp(argv[i], "-kg1")) {
            if(i + 1 >= argc)
                FatalError("-kg1 requires an argument\n");
            G1(inputState) = getCharsetByName(argv[i + 1]);
            i += 2;
        } else if(!strcmp(argv[i], "-kg2")) {
            if(i + 1 >= argc)
                FatalError("-kg2 requires an argument\n");
            G2(inputState) = getCharsetByName(argv[i + 1]);
            i += 2;
        } else if(!strcmp(argv[i], "-kg3")) {
            if(i + 1 >= argc)
                FatalError("-kg3 requires an argument\n");
            G3(inputState) = getCharsetByName(argv[i + 1]);

            i += 2;
        } else if(!strcmp(argv[i], "-kgl")) {
            int j;
            if(i + 1 >= argc)
                FatalError("-kgl requires an argument\n");
            if(strlen(argv[i + 1]) != 2 ||
               argv[i + 1][0] != 'g')
                j = -1;
            else 
                j = argv[i + 1][1] - '0';
            if(j < 0 || j > 3)
                FatalError("The argument of -kgl "
                           "should be one of g0 through g3,\n"
                           "not %s\n", argv[i + 1]);
            else
                inputState->glp = &inputState->g[j];
            i += 2;
        } else if(!strcmp(argv[i], "-kgr")) {
            int j;
            if(i + 1 >= argc)
                FatalError("-kgl requires an argument\n");
            if(strlen(argv[i + 1]) != 2 ||
               argv[i + 1][0] != 'g')
                j = -1;
            else 
                j = argv[i + 1][1] - '0';
            if(j < 0 || j > 3)
                FatalError("The argument of -kgl "
                           "should be one of g0 through g3,\n"
                           "not %s\n", argv[i + 1]);
            else
                inputState->grp = &inputState->g[j];
            i += 2;
        } else if(!strcmp(argv[i], "-argv0")) {
            if(i + 1 >= argc)
                FatalError("-argv0 requires an argument\n");
            child_argv0 = argv[i + 1];
            i += 2;
        } else if(!strcmp(argv[i], "-x")) {
            exitOnChild = 1;
            i++;
        } else if(!strcmp(argv[i], "-c")) {
            converter = 1;
            i++;
        } else if(!strcmp(argv[i], "-ilog")) {
            if(i + 1 >= argc)
                FatalError("-ilog requires an argument\n");
            ilog = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if(ilog < 0) {
                perror("Couldn't open input log");
                exit(1);
            }
            i += 2;
        } else if(!strcmp(argv[i], "-olog")) {
            if(i + 1 >= argc)
                FatalError("-olog requires an argument\n");
            olog = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if(olog < 0) {
                perror("Couldn't open output log");
                exit(1);
            }
            i += 2;
        } else if(!strcmp(argv[i], "-encoding")) {
            int rc;
            if(i + 1 >= argc)
                FatalError("-encoding requires an argument\n");
            rc = initIso2022(NULL, argv[i + 1], outputState);
            if(rc < 0)
                FatalError("Couldn't init output state\n");
            i += 2;
        } else {
            FatalError("Unknown option %s\n", argv[i]);
        }
    }
    return i;
}

static int
parseArgs(int argc, char **argv, char *argv0,
          char **path_return, char ***argv_return)
{
    char *path = NULL;
    char **child_argv = NULL;

    if(argc <= 0) {
        char *shell;
        shell = getenv("SHELL");
        if(shell) {
            path = malloc(strlen(shell) + 1);
            if(!path)
                goto bail;
            strcpy(path, shell);
        } else {
            path = malloc(strlen("/bin/sh") + 1);
            if(!path)
                goto bail;
            strcpy(path, "/bin/sh");
        }
        child_argv = malloc(2 * sizeof(char*));
        if(!child_argv)
            goto bail;
        if(argv0)
            child_argv[0] = argv0;
        else
            child_argv[0] = my_basename(path);
        child_argv[1] = NULL;
    } else {
        path = malloc(strlen(argv[0]) + 1);
        if(!path)
            goto bail;
        strcpy(path, argv[0]);
        child_argv = malloc((argc + 1) * sizeof(char*));
        if(!child_argv) {
            goto bail;
        }
        if(child_argv0)
            child_argv[0] = argv0;
        else
            child_argv[0] = my_basename(argv[0]);
        memcpy(child_argv + 1, argv + 1, (argc - 1) * sizeof(char*));
        child_argv[argc] = NULL;
    }

    *path_return = path;
    *argv_return = child_argv;
    return 0;

  bail:
    if(path)
        free(path);
    if(argv)
        free(argv);
    return -1;
}
        

int
main(int argc, char **argv)
{
    int rc;
    int i;
    char *l;

    l = setlocale(LC_ALL, "");
    if(!l)
        ErrorF("Warning: couldn't set locale.\n");

    inputState = allocIso2022();
    if(!inputState)
        FatalError("Couldn't create input state\n");
        
    outputState = allocIso2022();
    if(!outputState)
        FatalError("Couldn't create output state\n");
    
    if(l) {
        locale_name = setlocale(LC_CTYPE, NULL);
    } else {
        locale_name = getenv("LC_ALL");
        if(locale_name == NULL) {
            locale_name = getenv("LC_CTYPE");
            if(locale_name == NULL) {
                locale_name = getenv("LANG");
            }
        }
    }

    if(locale_name == NULL) {
        ErrorF("Couldn't get locale name -- using C\n");
        locale_name = "C";
    }

    rc = initIso2022(locale_name, NULL, outputState);
    if(rc < 0)
        FatalError("Couldn't init output state\n");

    i = parseOptions(argc, argv);
    if(i < 0)
        FatalError("Couldn't parse options\n");

    rc = mergeIso2022(inputState, outputState);
    if(rc < 0)
        FatalError("Couldn't init input state\n");

    if(converter)
        return convert(0, 1);
    else
        return condom(argc - i, argv + i);
}

static int
convert(int ifd, int ofd)
{
    int rc, i;
    unsigned char buf[BUFFER_SIZE];

    rc = droppriv();
    if(rc < 0) {
        perror("Couldn't drop priviledges");
        exit(1);
    }

    while(1) {
        i = read(ifd, buf, BUFFER_SIZE);
        if(i <= 0) {
            if(i < 0) {
                perror("Read error");
                exit(1);
            }
            break;
        }
        copyOut(outputState, ofd, buf, i);
    }
    return 0;
}
        
static void
sigwinchHandler(int sig)
{
    sigwinch_queued = 1;
}

static void
sigchldHandler(int sig)
{
    sigchld_queued = 1;
}

static int
condom(int argc, char **argv)
{
    int pty;
    int pid;
    char *line;
    char *path;
    char **child_argv;
    int rc;
    int val;

    rc = parseArgs(argc, argv, child_argv0,
                   &path, &child_argv);
    if(rc < 0)
        FatalError("Couldn't parse arguments\n");

    rc = allocatePty(&pty, &line);
    if(rc < 0) {
        perror("Couldn't allocate pty");
        exit(1);
    }

    rc = droppriv();
    if(rc < 0) {
        perror("Couldn't drop priviledges");
        exit(1);
    }
#ifdef SIGWINCH
    installHandler(SIGWINCH, sigwinchHandler);
#endif
    installHandler(SIGCHLD, sigchldHandler);

    rc = copyTermios(0, pty);
    if(rc < 0)
        FatalError("Couldn't copy terminal settings\n");

    rc = setRawTermios();
    if(rc < 0)
        FatalError("Couldn't set terminal to raw\n");

    val = fcntl(0, F_GETFL, 0);
    if(val >= 0) {
        fcntl(0, F_SETFL, val | O_NONBLOCK);
    }
    val = fcntl(pty, F_GETFL, 0);
    if(val >= 0) {
        fcntl(pty, F_SETFL, val | O_NONBLOCK);
    }

    setWindowSize(0, pty);

    pid = fork();
    if(pid < 0) {
        perror("Couldn't fork");
        exit(1);
    }

    if(pid == 0) {
        close(pty);
#ifdef SIGWINCH
        installHandler(SIGWINCH, SIG_DFL);
#endif
        installHandler(SIGCHLD, SIG_DFL);
        child(line, path, child_argv);
    } else {
        free(child_argv);
        free(path);
        free(line);
        parent(pid, pty);
    }

    return 0;
}

void
child(char *line, char *path, char **argv)
{
    int tty;
    int pgrp;

    close(0);
    close(1);
    close(2);

    pgrp = setsid();
    if(pgrp < 0) {
        kill(getppid(), SIGABRT);
        exit(1);
    }

    tty = openTty(line);
    if(tty < 0) {
        kill(getppid(), SIGABRT);
        exit(1);
    }
    
    if(tty != 0)
        dup2(tty, 0);
    if(tty != 1)
        dup2(tty, 1);
    if(tty != 2)
        dup2(tty, 2);

    if(tty > 2)
        close(tty);
    
    execvp(path, argv);
    perror("Couldn't exec");
    exit(1);
}

void
parent(int pid, int pty)
{
    unsigned char buf[BUFFER_SIZE];
    int i;
    int rc;

    if(verbose) {
        reportIso2022(outputState);
    }

    for(;;) {
        rc = waitForInput(0, pty);

        if(sigwinch_queued) {
            sigwinch_queued = 0;
            setWindowSize(0, pty);
        }

        if(sigchld_queued && exitOnChild)
            break;

        if(rc > 0) {
            if(rc & 2) {
                i = read(pty, buf, BUFFER_SIZE);
                if((i == 0) || ((i < 0) && (errno != EAGAIN)))
                    break;
                if(i > 0)
                    copyOut(outputState, 0, buf, i);
            }
            if(rc & 1) {
                i = read(0, buf, BUFFER_SIZE);
                if((i == 0) || ((i < 0) && (errno != EAGAIN)))
                    break;
                if(i > 0)
                    copyIn(inputState, pty, buf, i);
            }
        }
    }

    restoreTermios();
}
