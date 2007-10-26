
#include <stdio.h>
#include <dns_sd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach-o/arch.h>
#include "trace.h"
#include "versinfo.h"

typedef struct _CompilerInfo {
    char *abs_path;          /* Absolute path to the compiler's executable */
    char *raw_path;          /* Path to compare to the client's compiler path */
    char *versionInfo;
    struct timespec modTime;
    struct _CompilerInfo *next;
} CompilerInfo;

/*
 Invokes the command line given by commandLine (using popen) and returns the
 text of the command's output. Returns NULL if any error is encountered.
 */
static char *dcc_run_simple_command(const char *commandLine)
{
    FILE *output;
    char *versionInfo = NULL;;
    int buffSize = MAXPATHLEN, len = 0;
    
    output = popen(commandLine, "r");
    if (output) {
        versionInfo = (char *)malloc(buffSize * sizeof(char));
        while (versionInfo && !feof(output)) {
            if (len == buffSize-1) {
                char *newBuff = (char *)realloc(versionInfo, buffSize*2);
                if (newBuff) {
                    versionInfo = newBuff;
                    buffSize *= 2;
                } else {
                    free(versionInfo);
                    return NULL;
                }
            }
            len += fread(&versionInfo[len], 1, buffSize-len-1, output);
        }
        pclose(output);
    }
    versionInfo[len]=0;
    return versionInfo;
}

/* Get the executable path. */
static const char *dcc_get_executable_path()
{
    static char exe_path[PATH_MAX];
    char buf[PATH_MAX];
    uint32_t bufsize = sizeof(buf);
    char *cp;

    if (*exe_path) return exe_path;
        
    if (_NSGetExecutablePath(buf, &bufsize) != 0) {
        rs_log_error("Cannot get executable path for compilers");
        return NULL;
    }

    cp = buf + strlen(buf);

    while (cp > buf && *--cp != '/') {} /* skip over progname */

    if (cp <= buf) {
        rs_log_error("Overran the path name");
        return NULL;
    }

    *cp = 0;

    if (realpath(buf, exe_path) == NULL) {
        rs_log_error("Cannot execute 'realpath()' for executable path");
        memset(exe_path, '\0', PATH_MAX);
        return NULL;
    }

    cp = exe_path + strlen(exe_path);

    while (cp > exe_path && *--cp != '/') {} /* skip over dir (usually "bin") */

    if (cp <= exe_path) {
        rs_log_error("Cannot skip over directories in executable path");
        memset(exe_path, '\0', PATH_MAX);
        return NULL;
    }

    *cp = 0;
    return exe_path;
}

static CompilerInfo *dcc_parse_distcc_compilers()
{
    /*
     * The file 'share/distcc_compilers' lists allowable compilers as relative
     * or absolute paths. The relative paths are relative to
     * <developer_usr_dir>.
     */
    static CompilerInfo *compilers = NULL;
    static const char *distcc_compilers_file = "share/distcc_compilers";
    static char distcc_path[PATH_MAX];
    struct stat sb;

    if (compilers) return compilers;

    const char *exe_path = dcc_get_executable_path();
    if (!exe_path) return NULL;

    if (!*distcc_path) {
        strlcpy(distcc_path, exe_path, sizeof(distcc_path));
        strlcat(distcc_path, "/", sizeof(distcc_path));
        strlcat(distcc_path, distcc_compilers_file, sizeof(distcc_path));
    }

    if (stat(distcc_path, &sb) != 0) {
        rs_log_error("distcc_compilers file not found on path '%s'!", exe_path);
        return NULL;
    }

    int compilersFD = open(distcc_path, O_RDONLY, 0);

    if (compilersFD == -1) {
        rs_log_error("Cannot open distcc_compilers file!");
        return NULL;
    }

    char *compilersBuff = (char *)malloc(sb.st_size + 1);

    if (!compilersBuff) {
        close(compilersFD);
        return NULL;
    }

    if (read(compilersFD, compilersBuff, sb.st_size) != sb.st_size) {
        free(compilersBuff);
        close(compilersFD);
        return NULL;
    }

    compilersBuff[sb.st_size] = '\0'; // null terminate

    // change all the newlines to terminators
    int i;

    for (i=0; i<sb.st_size; i++)
        if (compilersBuff[i] == '\n')
            compilersBuff[i] = '\0';

    // now we can just parse line by line
    int lineStart, lineLen, compilerCount = 0;
    for (lineStart = 0; lineStart < sb.st_size; lineStart += lineLen + 1) {
        lineLen = strlen(&compilersBuff[lineStart]);
        if (lineLen > 0 && compilersBuff[lineStart] != '#') {
            /*
             * We have not seen this compiler before so allocate a new node note
             * that we do not fill in the version info here
             */
            CompilerInfo *ci = (CompilerInfo *)calloc(1, sizeof(CompilerInfo));
            size_t buff_len = strlen(&compilersBuff[lineStart]) + 1;

            /* The "raw path" is unaltered from the 'distcc_compilers' file. It
             * is used during comparisons against the client's compiler path. */
            ci->raw_path = (char *)calloc(1, buff_len);
            strlcpy(ci->raw_path, &compilersBuff[lineStart], buff_len);

            if (*ci->raw_path != '/') {
                /* This is a relative path */
                char c_path[PATH_MAX];
                strlcpy(c_path, exe_path, sizeof(c_path));
                strlcat(c_path, "/", sizeof(c_path));
                strlcat(c_path, ci->raw_path, sizeof(c_path));
                ci->abs_path = strdup(c_path);
            } else {
                /* This is an absolute path */
                ci->abs_path = ci->raw_path;
            }

            struct stat cc_sb;
            if (stat(ci->abs_path, &cc_sb) == 0) {
                ci->next = compilers;
                compilers = ci;
            } else {
                if (ci->raw_path != ci->abs_path) free(ci->raw_path);
                free(ci->abs_path);
                free(ci);
            }
        }
    }

    free(compilersBuff);
    close(compilersFD);
    return compilers;
}

/*
 * For each compiler path in 'share/distcc_compilers':
 *
 *   - If it is a relative path, take the last two components of the compiler
 *     path from the client's incoming compiler path and compare it. If there is
 *     a match, use _NSGetExecutablePath() to get the path to that compiler,
 *     remove the last two path components to obtain the <developer_usr_dir>
 *     path prefix, apply it to the validated compiler subpath, and verify that
 *     the resulting path exists.
 *     
 *   - If it is an absoluate path and it's equal to the compiler path, then
 *     verify that that compiler exists and, if so, use it.
 */
static CompilerInfo *dcc_compiler_info_for_path(const char *compiler)
{
    CompilerInfo *compilers = dcc_parse_distcc_compilers();

    if (!compilers) {
        rs_log_error("Couldn't find or parse 'distcc_compilers' file");
        return NULL;
    }

    /* Find the last two components of the client's compiler path */
    const char *cp = compiler;
    cp = strrchr(cp, '/');
    if (!cp) {
        rs_log_error("Invalid compiler path '%s'", compiler);
        return NULL;
    }

    while (*(cp - 1) != '/' && cp > compiler) --cp;

    /* Find the compiler in the list of acceptable compilers */
    for (; compilers; compilers = compilers->next) {
        const char *compiler_path = cp;

        if (*compilers->raw_path == '/')
            /* Absolute path. We want to compare against the full client's
             * path */
            compiler_path = compiler;

        if (strcmp(compilers->raw_path, compiler_path) == 0)
            break;
    }

    if (!compilers)
        rs_log_error("Couldn't find matching compiler for '%s'", compiler);

    return compilers;
}

static char *_dcc_get_compiler_version(CompilerInfo *compiler)
{
    char *result = NULL;
    struct stat sb;
    if (compiler) {
        const char *c_path = compiler->abs_path;
        if (stat(c_path, &sb) == 0 && compiler->versionInfo != NULL) {
            // we found the compiler, check that the timestamp is unchanged
            if (memcmp(&sb.st_ctimespec, &compiler->modTime, sizeof(struct timespec)) != 0) {
                // didn't match, so throw away version number
                rs_log_warning("compiler version changed: %s", c_path);
                //free(compiler->versionInfo); // leak; should be very uncommon
                compiler->versionInfo = NULL;
            }
        }
        
        if (!compiler->versionInfo) {
            // have to fetch the version number
            int lineLen = strlen(c_path);
            char *versionArgs = " -v 2>&1";
            char commandBuff[lineLen+strlen(versionArgs)+1];
            char *versionOutput;
            strcpy(commandBuff, c_path);
            strcat(commandBuff, versionArgs);
            versionOutput = dcc_run_simple_command(commandBuff);
            if (versionOutput) {
                char *version = strstr(versionOutput, "gcc version");
                if (version) {
                    int newline = 0;
                    while (version[newline] != '\n' && version[newline] != 0)
                        newline++;
                    compiler->versionInfo = (char *)malloc(newline+1);
                    strncpy(compiler->versionInfo, version, newline);
                    compiler->versionInfo[newline]=0;
                    compiler->modTime = sb.st_ctimespec;
                }
            }
        }
        result = compiler->versionInfo;
    }
    return result;
}

char *dcc_get_compiler_version(char *compilerPath)
{
    return _dcc_get_compiler_version(dcc_compiler_info_for_path(compilerPath));
}

int dcc_is_allowed_compiler(char *path)
{
    rs_trace("(dcc_is_allowed_compiler) allowed compiler path: %s", path);
    return dcc_compiler_info_for_path(path) != NULL;
}

/*
 This function returns a list of compiler version strings to use in the
 TXT record. It returns a NULL terminated array of version strings, or
 NULL if an error is encountered.
 */
char **dcc_get_all_compiler_versions(void)
{
    CompilerInfo *compilers = dcc_parse_distcc_compilers();
    char **result = NULL;
    if (result == NULL) {
        struct stat sb;
        CompilerInfo *ci;
        int i, j, count = 0;
        for (ci = compilers; ci != NULL; ci=ci->next)
            count++;
        
        result = (char **)calloc(count+1, sizeof(char *));
        for (i=0, ci = compilers; ci != NULL; ci = ci->next) {
            char *version = _dcc_get_compiler_version(ci);
            if (version) {
                for (j=0; j<i; j++) {
                    if (strcmp(result[j], version) == 0)
                        break;
                }
                if (j == i)
                    result[i++] = version;
            }
        }
    }
    return result;
}


char *dcc_get_system_version(void)
{
    static char *ret = NULL;
    if (ret == NULL) {
        char *sw_vers = dcc_run_simple_command("/usr/bin/sw_vers");
        if (sw_vers) {
            char *prodVers, *prodVersStr = "ProductVersion:";
            char *buildVers, *buildVersStr = "BuildVersion:";
            char *nl;
            char archbuf[32];
            
            prodVers = strstr(sw_vers, prodVersStr);
            buildVers = strstr(sw_vers, buildVersStr);

            if (prodVers) {
                // find the start of the actual version string
                prodVers += strlen(prodVersStr);
                while (isspace(*prodVers))
                    prodVers++;
                // change the newline to a null terminator
                nl = prodVers;
                while (*nl != 0 && *nl != '\n')
                    nl++;
                *nl = 0;
            } else {
                prodVers = "Unknown";
                rs_log_warning("failed to parse ProcuctVersion from sw_vers");
            }
            
            if (buildVers) {
                // find the start of the actual version string
                buildVers += strlen(buildVersStr);
                while (isspace(*buildVers))
                    buildVers++;
                // change the newline to a null terminator
                nl = buildVers;
                while (*nl != 0 && *nl != '\n')
                    nl++;
                *nl = 0;
            } else {
                buildVers = "Unknown";
                rs_log_warning("failed to parse BuildVersion from sw_vers");
            }
            
            const NXArchInfo *myArch = NXGetLocalArchInfo();
            const char *archName;
            if (myArch) {
                switch (myArch->cputype) {
                    case CPU_TYPE_POWERPC:
                        archName = "ppc";
                        break;
                    case CPU_TYPE_I386:
                        archName = "i386";
                        break;
                    default:
                        // don't know the name, just the number
                        archName = archbuf;
                        sprintf(archbuf, "%d", myArch->cputype);
                        rs_log_warning("unknown cputype: %d", myArch->cputype);
                        break;
                }
            } else {
                rs_log_warning("failed to get arch info");
                archName = "unknown";
            }
            
            // construct a string of the form 10.x.y (9A192, ppc)
            ret = malloc(strlen(prodVers) + strlen(buildVers) + strlen(archName) + strlen(" (, )") + 1);
            sprintf(ret, "%s (%s, %s)", prodVers, buildVers, archName);
            free(sw_vers);
        }
    }
    return ret;
}
