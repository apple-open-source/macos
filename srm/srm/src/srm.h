#define OPT_F       (1 << 0)
#define OPT_I       (1 << 1)
#define OPT_R       (1 << 2)
#define OPT_V       (1 << 3)
#define OPT_S       (1 << 4)
#define OPT_N       (1 << 5)
#define OPT_ZERO    (1 << 6)
#define OPT_VERIFY  (1 << 7)

#ifndef GLOBALS
extern int seclevel;
extern int options;
extern int opt_buffsize;
extern char *program_name;
#endif

void error(char *msg, ...);
void errorp(char *msg, ...);
int rename_unlink(const char*path);
void init_lcg(const long s1, const long s2);
double lcg(void);
int tree_walker(char ** trees);
int sunlink(const char * path);
void init_random(const unsigned int seed);
void seed_random(void);
char random_char(void);
void randomize_buffer(unsigned char *buffer, unsigned int length);
