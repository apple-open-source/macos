/* mod_dav lock database view utility,
** Keith Wannamaker 7/99, wannamak@us.ibm.com
**
** UUID code from ISO/DCE RPC spec and a former Internet Draft by Leach and Salz:
** http://www.ics.uci.edu/pub/ietf/webdav/uuid-guid/draft-leach-uuids-guids-01
**
*/

#include "../mod_dav.h"
#include "../dav_opaquelock.h"
#include "../sdbm/sdbm.h"
#include "../dav_fs_repos.h"

#ifdef WIN32
#define		R_OK 0x04
#endif

/* Include #defines and typedefs from dav_fs_lock.c */
#define DAV_LOCK_DIRECT		1
#define DAV_LOCK_INDIRECT	2

#define DAV_TYPE_INODE		10
#define DAV_TYPE_FNAME		11

typedef struct dav_lock_discovery_fixed
{
    char scope;
    char type;
    int depth;
    time_t timeout;
} dav_lock_discovery_fixed;
/* /Include */

char *show_uuid(dav_uuid_t *u)
{
	char *buf = calloc(100, sizeof(char));

	sprintf(buf, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		       u->time_low, u->time_mid, u->time_hi_and_version,
		       u->clock_seq_hi_and_reserved, u->clock_seq_low,
		       u->node[0], u->node[1], u->node[2],
		       u->node[3], u->node[4], u->node[5]);
	return(buf);
}

void display_key(datum key)
{
    if (key.dptr == 0 || key.dsize == 0) {
	printf("### NULL key ###");
	return;
    }
    switch(*key.dptr++)
    {
    case DAV_TYPE_INODE:
	printf("inode:%li", *((ino_t *)key.dptr));
	break;
    case DAV_TYPE_FNAME:
	printf("%s", key.dptr);
	break;
    default:
	printf("### Invalid key type (%u) ###", *--key.dptr);
	break;
    }
}

int display_info(datum key, datum val)
{
	char *ptr;
	int i = 0;
	dav_lock_discovery_fixed ld;
	dav_uuid_t locktoken;
	time_t expires;

	if (val.dsize == 0)
		return(0);

	ptr = val.dptr;

	while(i < val.dsize) {
		switch (*(ptr + i++)) {
		case DAV_LOCK_DIRECT:
		{
			char *owner;
			char *auth_user;

			/* Create and fill a lock_discovery structure */

			memcpy(&ld, ptr + i, sizeof(ld));
			i += sizeof(ld);
			memcpy(&locktoken, ptr + i, sizeof(locktoken));
			i += sizeof(locktoken);
			owner = ptr + i;
			i += strlen(owner) + 1;
			auth_user = ptr + i;
			i += strlen(auth_user) + 1;
			printf(" -- [");
			display_key(key);
			printf("]: direct lock [%s,%s]",
				ld.scope == DAV_LOCKSCOPE_EXCLUSIVE ? "exclusive" : "shared",
				ld.type  == DAV_LOCKTYPE_WRITE      ? "write"     : "unknown");
			if(ld.depth == DAV_INFINITY)
				printf(" depth infinite\n");
			else
				printf(" depth %i\n", ld.depth);
			if (ld.timeout == 0) {
			    printf("    Timeout  : infinite\n");
			}
			else {
			    char *foo;
			    foo = ctime(&ld.timeout);
			    foo[strlen(foo)-1] = 0;
			    printf("    Timeout  : [%s] (in %li seconds)\n",
				   foo, ld.timeout - time(NULL));
			}
			printf("    Token    : [%s]\n"
			       "    Owner    : \"%s\"\n"
			       "    Auth_User: \"%s\"\n",
			       show_uuid(&locktoken),
			       owner[0] == 10 ? "" : owner,
			       auth_user);
			break;
		}

		case DAV_LOCK_INDIRECT:
		{
			datum keyref = { 0 };

			/* Fill a dav_datum with this info, recurse to fill
			 * information from that entry in the database */

			memcpy(&locktoken, ptr + i, sizeof(uuid_t));
			i += sizeof(uuid_t);
			memcpy(&expires, ptr + i, sizeof(time_t));
			i += sizeof(time_t);
			memcpy(&keyref.dsize, ptr + i, sizeof(int));
			i += sizeof(int);
			keyref.dptr = ptr + i;
			i += keyref.dsize;

			printf(" -- [");
			display_key(key);
			printf("]: indirect lock by [");
			display_key(keyref);
			printf("]\n");
			if (expires == 0) {
			    printf("    Timeout  : infinite\n");
			}
			else {
			    char *foo;
			    foo = ctime(&expires);
			    foo[strlen(foo)-1] = 0;
			    printf("    Timeout  : [%s] (in %li seconds)\n",
				   foo, expires - time(NULL));
			}
			printf("    Token    : [%s]\n", show_uuid(&locktoken));
			
			break;
		}

		default:
			printf("### Invalid token\n");
			return -1;
		}
	}

	return 0;
}

/* Test for existence and read perm */
int CheckFile(char *argv[], char *ext)
{
        char filename[255];

	strcpy(filename, argv[1]);
	strcat(filename, ext);
	
	if (access(filename, R_OK) != 0) {
		printf("%s: cannot read file %s\n", argv[0], filename);
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	DBM *db;
	datum key, val;
	int count = 1;

	if (argc != 2) {
                printf("%s - dumps mod_dav lock database\n", argv[0]);
		printf("%s: no database specified (don't include extension)\n", argv[0]);
		return(-1);
	}

	if (CheckFile(argv, ".pag"))
		return(-1);
	if (CheckFile(argv, ".dir"))
		return(-1);
		
	db = sdbm_open((char *) argv[1],
		     O_RDONLY | O_BINARY, DAV_FS_MODE_FILE);

	key = sdbm_firstkey(db);
	if (key.dsize == 0) {
	    printf("%s: no outstanding locks.\n", argv[0]);
	}
	else {
	    while(key.dsize) {
		printf("[Lock record #%3d]\n", count++);
		val = sdbm_fetch(db, key);
		if (display_info(key, val) == -1)
		    return(-1);
		key = sdbm_nextkey(db);
	    }
	}

	sdbm_close(db);
	exit(0);
}
