

void parse_init(void);
int parse(FILE **);
int parse_string(char *);

int setkeymsg(char *, size_t *);
int sendkeymsg(char *, size_t);

int yylex(void);
int yyparse(void);
void yyfatal(const char *);
void yyerror(const char *);

extern int f_rfcmode;
extern int lineno;
extern int last_msg_type;
extern u_int32_t last_priority;
extern int exit_now;

extern u_char m_buf[BUFSIZ];
extern u_int m_len;
extern int f_debug;

#ifdef HAVE_PFKEY_POLICY_PRIORITY
extern int last_msg_type;
extern u_int32_t last_priority;
#endif
