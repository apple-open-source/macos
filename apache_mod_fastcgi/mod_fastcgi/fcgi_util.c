/*
 * $Id: fcgi_util.c,v 1.30 2003/10/30 01:08:34 robs Exp $
 */

#include "fcgi.h"

#ifdef WIN32
#pragma warning( disable : 4100 )
#elif defined(APACHE2)
#include <netdb.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "unixd.h"
#endif

uid_t 
fcgi_util_get_server_uid(const server_rec * const s)
{
#if defined(WIN32) 
    return (uid_t) 0;
#elif defined(APACHE2)
    /* the main server's uid */
    return ap_user_id;
#else
    /* the vhost's uid */
    return s->server_uid;
#endif
}

uid_t 
fcgi_util_get_server_gid(const server_rec * const s)
{
#if defined(WIN32) 
    return (uid_t) 0;
#elif defined(APACHE2)
    /* the main server's gid */
    return ap_group_id;
#else
    /* the vhost's gid */
    return s->server_gid;
#endif
}
 
/*******************************************************************************
 * Compute printable MD5 hash. Pool p is used for scratch as well as for
 * allocating the hash - use temp storage, and dup it if you need to keep it.
 */
char *
fcgi_util_socket_hash_filename(pool *p, const char *path,
        const char *user, const char *group)
{
    char *buf = apr_pstrcat(p, path, user, group, NULL);

    /* Canonicalize the path (remove "//", ".", "..") */
    ap_getparents(buf);

    return ap_md5(p, (unsigned char *)buf);
}


 /*******************************************************************************
  * Concat src1 and src2 using the approprate path seperator for the platform. 
  */
static char * make_full_path(pool *a, const char *src1, const char *src2)
{
#ifdef WIN32
    register int x;
    char * p ;
    char * q ;

    x = strlen(src1);

    if (x == 0) {
	    p = apr_pstrcat(a, "\\", src2, NULL);
    }
    else if (src1[x - 1] != '\\' && src1[x - 1] != '/') {
	    p = apr_pstrcat(a, src1, "\\", src2, NULL);
    }
    else {
	    p = apr_pstrcat(a, src1, src2, NULL);
    }

    q = p ;
    while (*q)
	{
        if (*q == '/') {
	        *q = '\\' ;
        }
	    ++q;
	}

    return p ;
#else
    return ap_make_full_path(a, src1, src2);
#endif
}

/*******************************************************************************
 * Return absolute path to file in either "regular" FCGI socket directory or
 * the dynamic directory.  Result is allocated in pool p.
 */
const char *
fcgi_util_socket_make_path_absolute(pool * const p, 
        const char *const file, const int dynamic)
{
#ifdef APACHE2
    if (ap_os_is_path_absolute(p, (char *) file))
#else
    if (ap_os_is_path_absolute(file))
#endif
    {
	    return file;
    }
    else
    {
        const char * parent_dir = dynamic ? fcgi_dynamic_dir : fcgi_socket_dir;
        return (const char *) make_full_path(p, parent_dir, file);
    }
}

#ifndef WIN32
/*******************************************************************************
 * Build a Domain Socket Address structure, and calculate its size.
 * The error message is allocated from the pool p.  If you don't want the
 * struct sockaddr_un also allocated from p, pass it preallocated (!=NULL).
 */
const char *
fcgi_util_socket_make_domain_addr(pool *p, struct sockaddr_un **socket_addr,
        int *socket_addr_len, const char *socket_path)
{
    int socket_pathLen = strlen(socket_path);

    if (socket_pathLen >= sizeof((*socket_addr)->sun_path)) {
        return apr_pstrcat(p, "path \"", socket_path,
                       "\" is too long for a Domain socket", NULL);
    }

    if (*socket_addr == NULL)
        *socket_addr = apr_pcalloc(p, sizeof(struct sockaddr_un));
    else
        memset(*socket_addr, 0, sizeof(struct sockaddr_un));

    (*socket_addr)->sun_family = AF_UNIX;
    strcpy((*socket_addr)->sun_path, socket_path);

    *socket_addr_len = SUN_LEN(*socket_addr);
    return NULL;
}
#endif

/*******************************************************************************
 * Convert a hostname or IP address string to an in_addr struct.
 */
static int 
convert_string_to_in_addr(const char * const hostname, struct in_addr * const addr)
{
    struct hostent *hp;
    int count;

    addr->s_addr = inet_addr((char *)hostname);

#if !defined(INADDR_NONE) && defined(APACHE2)
#define INADDR_NONE APR_INADDR_NONE
#endif
    
    if (addr->s_addr == INADDR_NONE) {
        if ((hp = gethostbyname((char *)hostname)) == NULL)
            return -1;

        memcpy((char *) addr, hp->h_addr, hp->h_length);
        count = 0;
        while (hp->h_addr_list[count] != 0)
            count++;

        return count;
    }
    return 1;
}


/*******************************************************************************
 * Build an Inet Socket Address structure, and calculate its size.
 * The error message is allocated from the pool p. If you don't want the
 * struct sockaddr_in also allocated from p, pass it preallocated (!=NULL).
 */
const char *
fcgi_util_socket_make_inet_addr(pool *p, struct sockaddr_in **socket_addr,
        int *socket_addr_len, const char *host, unsigned short port)
{
    if (*socket_addr == NULL)
        *socket_addr = apr_pcalloc(p, sizeof(struct sockaddr_in));
    else
        memset(*socket_addr, 0, sizeof(struct sockaddr_in));

    (*socket_addr)->sin_family = AF_INET;
    (*socket_addr)->sin_port = htons(port);

    /* Get an in_addr represention of the host */
    if (host != NULL) {
        if (convert_string_to_in_addr(host, &(*socket_addr)->sin_addr) != 1) {
            return apr_pstrcat(p, "failed to resolve \"", host,
                           "\" to exactly one IP address", NULL);
        }
    } else {
      (*socket_addr)->sin_addr.s_addr = htonl(INADDR_ANY);
    }

    *socket_addr_len = sizeof(struct sockaddr_in);
    return NULL;
}

/*******************************************************************************
 * Determine if a process with uid/gid can access a file with mode permissions.
 */
const char *
fcgi_util_check_access(pool *tp, 
        const char * const path, const struct stat *statBuf, 
        const int mode, const uid_t uid, const gid_t gid)
{
    struct stat myStatBuf;

    if (statBuf == NULL) {    
        if (stat(path, &myStatBuf) < 0)
            return apr_psprintf(tp, "stat(%s) failed: %s", path, strerror(errno));
        statBuf = &myStatBuf;
    }
    
#ifndef WIN32    
    /* If the uid owns the file, check the owner bits */
    if (uid == statBuf->st_uid) {
        if (mode & R_OK && !(statBuf->st_mode & S_IRUSR))
            return "read not allowed by owner";
        if (mode & W_OK && !(statBuf->st_mode & S_IWUSR))
            return "write not allowed by owner";
        if (mode & X_OK && !(statBuf->st_mode & S_IXUSR))
            return "execute not allowed by owner";
        return NULL;
    }
#else
    if (mode & _S_IREAD && !(statBuf->st_mode & _S_IREAD))
        return "read not allowed";
    if (mode & _S_IWRITE && !(statBuf->st_mode & _S_IWRITE))
        return "write not allowed";
    
    /* I don't think this works on FAT, but since I don't know how to check..
     * if (mode & _S_IEXEC && !(statBuf->st_mode & _S_IEXEC))
     *     return "execute not allowed"; */
#endif

#if  !defined(__EMX__) && !defined(WIN32)
    /* If the gid is same as the file's group, check the group bits */
    if (gid == statBuf->st_gid) {
        if (mode & R_OK && !(statBuf->st_mode & S_IRGRP))
            return "read not allowed by group";
        if (mode & W_OK && !(statBuf->st_mode & S_IWGRP))
            return "write not allowed by group";
        if (mode & X_OK && !(statBuf->st_mode & S_IXGRP))
            return "execute not allowed by group";
        return NULL;
    }

    /* Get the user membership for the file's group.  If the
     * uid is a member, check the group bits. */
    {
        const struct group * const gr = getgrgid(statBuf->st_gid);
        const struct passwd * const pw = getpwuid(uid);

        if (gr != NULL && pw != NULL) {
            char **user = gr->gr_mem;
            for ( ; *user != NULL; user++) {
                if (strcmp(*user, pw->pw_name) == 0) {
                    if (mode & R_OK && !(statBuf->st_mode & S_IRGRP))
                        return "read not allowed by group";
                    if (mode & W_OK && !(statBuf->st_mode & S_IWGRP))
                        return "write not allowed by group";
                    if (mode & X_OK && !(statBuf->st_mode & S_IXGRP))
                        return "execute not allowed by group";
                    return NULL;
                }
            }
        }
    }
    
    /* That just leaves the other bits.. */
    if (mode & R_OK && !(statBuf->st_mode & S_IROTH))
        return "read not allowed";
    if (mode & W_OK && !(statBuf->st_mode & S_IWOTH))
        return "write not allowed";
    if (mode & X_OK && !(statBuf->st_mode & S_IXOTH))
        return "execute not allowed";
#endif

    return NULL;
}


/*******************************************************************************
 * Find a FastCGI server with a matching fs_path, and if fcgi_wrapper is
 * enabled with matching uid and gid.
 */
fcgi_server *
fcgi_util_fs_get_by_id(const char *ePath, uid_t uid, gid_t gid)
{
    char path[FCGI_MAXPATH];
    fcgi_server *s;

    /* @@@ This should now be done in the loop below */
    apr_cpystrn(path, ePath, FCGI_MAXPATH);
    ap_no2slash(path);

    for (s = fcgi_servers; s != NULL; s = s->next) {
        int i;
        const char *fs_path = s->fs_path;
        for (i = 0; fs_path[i] && path[i]; ++i) {
            if (fs_path[i] != path[i]) {
                break;
            }
        }
        if (fs_path[i]) {
            continue;
        }
        if (path[i] == '\0' || path[i] == '/') {
        if (fcgi_wrapper == NULL || (uid == s->uid && gid == s->gid))
            return s;
        }
    }
    return NULL;
}

/*******************************************************************************
 * Find a FastCGI server with a matching fs_path, and if fcgi_wrapper is
 * enabled with matching user and group.
 */
fcgi_server *
fcgi_util_fs_get(const char *ePath, const char *user, const char *group)
{
    char path[FCGI_MAXPATH];
    fcgi_server *s;

    apr_cpystrn(path, ePath, FCGI_MAXPATH);
    ap_no2slash(path);
    
    for (s = fcgi_servers; s != NULL; s = s->next) {
        if (strcmp(s->fs_path, path) == 0) {
            if (fcgi_wrapper == NULL)
                return s;

            if (strcmp(user, s->user) == 0 
                && (user[0] == '~' || strcmp(group, s->group) == 0))
            {
                return s;
            }
        }
    }
    return NULL;
}

const char *
fcgi_util_fs_is_path_ok(pool * const p, const char * const fs_path, struct stat *finfo)
{
    const char *err;

    if (finfo == NULL) {
        finfo = (struct stat *)apr_palloc(p, sizeof(struct stat));	        
        if (stat(fs_path, finfo) < 0)
            return apr_psprintf(p, "stat(%s) failed: %s", fs_path, strerror(errno));
    }

    /* No Parse Header scripts aren't allowed.
     * @@@ Well... we really could quite easily */ 
    if (strncmp(strrchr(fs_path, '/'), "/nph-", 5) == 0)
        return apr_psprintf(p, "NPH scripts cannot be run as FastCGI");
    
    if (finfo->st_mode == 0) 
        return apr_psprintf(p, "script not found or unable to stat()");

    if (S_ISDIR(finfo->st_mode)) 
        return apr_psprintf(p, "script is a directory!");
    
    /* Let the wrapper determine what it can and can't execute */
    if (! fcgi_wrapper)
    {
#ifdef WIN32
        err = fcgi_util_check_access(p, fs_path, finfo, _S_IEXEC, fcgi_user_id, fcgi_group_id);
#else
        err = fcgi_util_check_access(p, fs_path, finfo, X_OK, fcgi_user_id, fcgi_group_id);
#endif
        if (err) {
            return apr_psprintf(p,
                "access for server (uid %ld, gid %ld) not allowed: %s",
                (long)fcgi_user_id, (long)fcgi_group_id, err);
        }
    }
    
    return NULL;
}



/*******************************************************************************
 * Allocate a new FastCGI server record from pool p with default values.
 */
fcgi_server *
fcgi_util_fs_new(pool *p)
{
    fcgi_server *s = (fcgi_server *) apr_pcalloc(p, sizeof(fcgi_server));

    /* Initialize anything who's init state is not zeroizzzzed */
    s->listenQueueDepth = FCGI_DEFAULT_LISTEN_Q;
    s->appConnectTimeout = FCGI_DEFAULT_APP_CONN_TIMEOUT;
    s->idle_timeout = FCGI_DEFAULT_IDLE_TIMEOUT;
    s->initStartDelay = DEFAULT_INIT_START_DELAY;
    s->restartDelay = FCGI_DEFAULT_RESTART_DELAY;
    s->restartOnExit = FALSE;
    s->directive = APP_CLASS_UNKNOWN;
    s->processPriority = FCGI_DEFAULT_PRIORITY;
    s->envp = &fcgi_empty_env;
    
#ifdef WIN32
    s->listenFd = (int) INVALID_HANDLE_VALUE;
#else
    s->listenFd = -2;
#endif

    return s;
}

/*******************************************************************************
 * Add the server to the linked list of FastCGI servers.
 */
void 
fcgi_util_fs_add(fcgi_server *s)
{
    s->next = fcgi_servers;
    fcgi_servers = s;
}

/*******************************************************************************
 * Configure uid, gid, user, group, username for wrapper.
 */
const char *
fcgi_util_fs_set_uid_n_gid(pool *p, fcgi_server *s, uid_t uid, gid_t gid)
{
#ifndef WIN32

    struct passwd *pw;
    struct group  *gr;

    if (fcgi_wrapper == NULL)
        return NULL;

    if (uid == 0 || gid == 0) {
        return "invalid uid or gid, see the -user and -group options";
    }

    s->uid = uid;
    pw = getpwuid(uid);
    if (pw == NULL) {
        return apr_psprintf(p,
            "getpwuid() couldn't determine the username for uid '%ld', "
            "you probably need to modify the User directive: %s",
            (long)uid, strerror(errno));
    }
    s->user = apr_pstrdup(p, pw->pw_name);
    s->username = s->user;

    s->gid = gid;
    gr = getgrgid(gid);
    if (gr == NULL) {
        return apr_psprintf(p,
            "getgrgid() couldn't determine the group name for gid '%ld', "
            "you probably need to modify the Group directive: %s",
            (long)gid, strerror(errno));
    }
    s->group = apr_pstrdup(p, gr->gr_name);

#endif /* !WIN32 */

    return NULL;
}

/*******************************************************************************
 * Allocate an array of ServerProcess records.
 */
ServerProcess *
fcgi_util_fs_create_procs(pool *p, int num)
{
    int i;
    ServerProcess *proc = (ServerProcess *)apr_pcalloc(p, sizeof(ServerProcess) * num);

    for (i = 0; i < num; i++) {
#ifdef WIN32
        proc[i].handle = INVALID_HANDLE_VALUE;
        proc[i].terminationEvent = INVALID_HANDLE_VALUE;
#endif
        proc[i].pid = 0;
        proc[i].state = FCGI_READY_STATE;
    }
    return proc;
}

int fcgi_util_ticks(struct timeval * tv) 
{
#ifdef WIN32
    /* millisecs is sufficent granularity */
    DWORD millis = GetTickCount();

    tv->tv_sec = millis / 1000;
    tv->tv_usec = (millis % 1000) * 1000;

    return 0;
#else
    return gettimeofday(tv, NULL);
#endif
}


