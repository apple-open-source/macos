#ifdef GNU_LEX_YACC
struct FlexState {
    int yy_start;
    int yy_init;
    int yy_more_flag;
    int yy_more_len;
    int yy_did_buffer_switch_on_eof;
    int yy_last_accepting_state;
    char *yy_last_accepting_cpos;
    void *yy_current_buffer;
#if 1
    char yy_hold_char;
    int yy_n_chars;
    char* yy_c_buf_p;
#endif

#if 0
	yy_state_type* yy_state_buf;
	yy_state_type* yy_state_ptr;

	char* yy_full_match;
	int* yy_full_state;
	int yy_full_lp;

	int yy_lp;
	int yy_looking_for_trail_begin;
#endif

    FILE *yyin;
    FILE *yyout;
    int yylineno;
    YYSTYPE yylval;
    int yyleng;
    char *yytext;
};
#endif
