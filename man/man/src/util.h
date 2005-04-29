/* functions and variables exported from util.c */

void get_permissions (void);
void no_privileges (void);
char *my_malloc (int n);
char *my_strdup (const char *s);
const char *mkprogname (const char *s);
int is_newer (const char *fa, const char *fb);
int do_system_command (const char *cmd, int silent);
FILE *my_popen(const char *cmd, const char *type);
char *my_xsprintf(char *f,...);

extern int ruid, rgid, euid, egid, suid;
