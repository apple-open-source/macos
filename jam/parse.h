/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * parse.h - make and destroy parse trees as driven by the parser
 */

/*
 * parse tree node
 */

typedef struct _PARSE PARSE;

struct _PARSE {
	void	(*func)();
	PARSE	*left;
	PARSE	*right;
	char	*string;
	char	*string1;
	LIST	*llist;
	LIST	*rlist;
	int	num;
} ;

void 	parse_file();
void 	parse_save();
PARSE	*parse_make();
void	parse_free();
