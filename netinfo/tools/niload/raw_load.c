#include <stdio.h>
#include <stdlib.h>
#include <netinfo/ni.h>
#include <NetInfo/nilib2.h>
#include <NetInfo/dsrecord.h>

#define forever for(;;)
#define META_IS_UNDERSCORE

typedef enum
{
	TokenNULL = 0,
	TokenLeftBrace = 1,
	TokenRightBrace = 2,
	TokenLeftParen = 3,
	TokenRightParen = 4,
	TokenComma = 5,
	TokenSemi = 6,
	TokenEqual = 7,
	TokenWord = 8,
	TokenCHILDREN = 9
} token_type_t;

int line_number = 1;
int record_number = 0;

char *
indent_string(int l)
{
	static char line[256];
	int i;

	for (i = 0; i < 256; i++) line[i] = ' ';
	line[l * 4] = '\0';
	return line;
}

typedef struct
{
	token_type_t type;
	char *value;
	
} token_t;

token_t **tstack;
unsigned int tcount = 0;

dsrecord **store;
unsigned int store_count = 0;

FILE *fp;

char *
token_string(token_t *t)
{
	if (t == NULL) return "NULL";

	switch (t->type)
	{
		case TokenNULL: return "-nil-";
		case TokenLeftBrace: return "{";
		case TokenRightBrace: return "}";
		case TokenLeftParen: return "(";
		case TokenRightParen: return ")";
		case TokenComma: return ",";
		case TokenSemi: return ";";
		case TokenEqual: return "=";
		case TokenCHILDREN: return "CHILDREN";
		case TokenWord: return t->value;
	}
	return "?";
}

void
freeToken(token_t *t)
{
	if (t == NULL) return;
	if (t->value != NULL) free(t->value);
	t->value = NULL;
	t->type = TokenNULL;
	free(t);
}

/*
 * Tokens are the characters: { } ( ) , ; =
 * strings within double quotes, and a run of any non-white characters.
 * White space includes spaces, tabs, and newlines.
 */
token_t *
get_token_1()
{
	token_t *t;
	char c;
	static char x = EOF;
	int run, quote, escape, len;

	t = (token_t *)malloc(sizeof(token_t));
	t->type = TokenNULL;
	t->value = NULL;
	
	/* Skip white space */
	run = 1;
	while (run == 1)
	{
		c = x;
		x = EOF;
		if (c == EOF) c = getc(fp);
		if (c == EOF) return t;
		if (c == ' ') continue;
		if (c == '\t') continue;
		if (c == '\n')
		{
			line_number++;
			continue;
		}
		run = 0;
	}
	
	if ((c == '{') || (c == '}')
		|| (c == '(') || (c == ')')
		|| (c == ',') || (c == ';') || (c == '='))
	{
		if (c == '{') t->type = TokenLeftBrace;
		else if (c == '}') t->type = TokenRightBrace;
		else if (c == '(') t->type = TokenLeftParen;
		else if (c == ')') t->type = TokenRightParen;
		else if (c == ',') t->type = TokenComma;
		else if (c == ';') t->type = TokenSemi;
		else if (c == '=') t->type = TokenEqual;

		return t;
	}

	escape = 0;
	if (c == '\\') escape = 1;
	quote = 0;
	if (c == '"') quote = 1;

	len = 1;
	t->value = malloc(len + 1);
	t->value[0] = c;
	t->value[len] = '\0';
	t->type = TokenWord;

	run = 1;
	while (run == 1)
	{
		c = getc(fp);
		if (c == EOF) return t;
		if (c == '\n') line_number++;
		if (escape == 0)
		{
			if (quote == 0)
			{
				if ((c == ' ') || (c == '\t') || (c == '\n')) return t;
				if ((c == '{') || (c == '}')
					|| (c == '(') || (c == ')')
					|| (c == ',') || (c == ';')
					|| (c == '"'))
				{
					x = c;
					return t;
				}
			}

			if ((quote == 1) && (c == '"')) run = 0;
		}
		escape = 0;
		if (c == '\\')
		{
			escape = 1;
			continue;
		}

		t->value[len++] = c;
		t->value = realloc(t->value, len + 1);
		t->value[len] = '\0';
	}

	return t;
}	

void
push_token(token_t *t)
{
	if (tcount == 0)
	{
		tstack = (token_t **)malloc(sizeof(token_t *));
	}
	else
	{
		tstack = (token_t **)realloc(tstack, (tcount + 1) * sizeof(token_t *));
	}

	tstack[tcount] = t;
	tcount++;
}

void
store_record(dsrecord *r)
{
	if (store_count == 0)
	{
		store = (dsrecord **)malloc(sizeof(dsrecord *));
	}
	else
	{
		store = (dsrecord **)realloc(store, (store_count + 1) * sizeof(dsrecord *));
	}

	store[store_count] = dsrecord_retain(r);
	store_count++;
}

dsrecord *
fetch_record(unsigned int n)
{
	int i;

	for (i = 0; i < store_count; i++)
	{
		if (store[i]->dsid == n) return dsrecord_retain(store[i]);
	}
	return NULL;
}

void
store_clean()
{
	int i;

	for (i = 0; i < store_count; i++) dsrecord_release(store[i]);
	if (store_count > 0) free(store);
}

token_t *
pop_token()
{
	token_t *t;
	int i;

	if (tcount > 0)
	{
		tcount--;
		t = tstack[tcount];
		if (tcount == 0)
		{
			free(tstack);
		}
		else
		{
			tstack = (token_t **)realloc(tstack, tcount * sizeof(token_t *));
		}

		return t;
	}
		
	t = get_token_1();
	if (t->type != TokenWord)
	{
		return t;
	}

	if (!strcmp(t->value, "CHILDREN"))
	{
		t->type = TokenCHILDREN;
		return t;
	}

	if (t->value[0] == '"')
	{
		for (i = 1; t->value[i] != '\0'; i++)
			t->value[i - 1] = t->value[i];
		i--;
		t->value[i] = '\0';
		i--;
		if (t->value[i] == '"') t->value[i] = '\0';
	}

	return t;
}

/*
 * dir ::= [{] proplist [subdirs] [}]
 * proplist ::= prop* 
 * prop ::= key = val ;
 * key ::= word
 * word :: = ["] charstring ["]
 * val :: = [(] word [, word]* [)]
 * subdirs ::= subdirkey = [(] dir [, dir]* [)] [;]
 * subdirkey ::= CHILDREN
 * dir ::= number
*/

token_t *
get_list_token(int *paren)
{
	token_t *t;

	t = pop_token();
	if (t->type == TokenLeftParen)
	{
		if (*paren == -1)
		{
			*paren = 1;
			freeToken(t);
			return get_list_token(paren);
		}
		else
		{
			fprintf(stderr, "Unexpected left paren \"(\" at line %d\n", line_number);
			freeToken(t);
			return NULL;
		}
	}
	else if (t->type == TokenRightParen)
	{
		if (*paren != 1)
		{
			fprintf(stderr, "Unexpected right paren \")\" at line %d\n", line_number);
			freeToken(t);
			return NULL;
		}

		t->type = TokenNULL;
		return t;
	}
	else if (t->type == TokenSemi)
	{
		if (*paren == 1)
		{
			fprintf(stderr, "Expecting right paren \")\" at line %d\n", line_number);
			freeToken(t);
			return NULL;
		}

		push_token(t);

		t = (token_t *)malloc(sizeof(token_t));
		t->type = TokenNULL;
		return t;
	}
	else if (t->type == TokenComma)
	{
		freeToken(t);
		return get_list_token(paren);
	}
	else if (t->type != TokenWord)
	{
		fprintf(stderr, "Unexpected token \"%s\" at line %d\n", token_string(t), line_number);
		freeToken(t);
		return NULL;
	}

	if (*paren == -1) *paren = 0;
	return t;
}

dsrecord *
get_dir(int level)
{
	token_t *t;
	int braces, paren, x;
	dsrecord *r, *k;
	dsattribute *a;
	dsdata *d;

	r = dsrecord_new();
	r->dsid = record_number++;
	if (level == 0) r->super = r->dsid;

	t = pop_token();
	if (t->type == TokenNULL)
	{
		freeToken(t);
		return r;
	}

	braces = 0;
	if (t->type == TokenLeftBrace)
	{
		freeToken(t);
		braces = 1;
	}
	else push_token(t);

	forever
	{
		t = pop_token();
		if (t->type == TokenRightBrace)
		{
			freeToken(t);
			if (braces == 1) return r;
			else
			{
				fprintf(stderr, "Unexpected right brace \"}\" at line %d\n", line_number);
				dsrecord_release(r);
				return NULL;
			}
		}
		else if (t->type == TokenWord)
		{
			d = cstring_to_dsdata(t->value);
			a = dsattribute_new(d);
			dsdata_release(d);
			freeToken(t);
			dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);

			t = pop_token();
			if (t->type != TokenEqual)
			{
				fprintf(stderr, "Expecting equal \"=\" at line %d\n", line_number);
				freeToken(t);
				dsrecord_release(r);
				return NULL;
			}
			freeToken(t);

			paren = -1;
			t = get_list_token(&paren);
			forever
			{
				if (t == NULL)
				{
					dsrecord_release(r);
					return NULL;
				}
				else if (t->type == TokenNULL) break;

				d = cstring_to_dsdata(t->value);
				dsattribute_append(a, d);
				dsdata_release(d);
				freeToken(t);
				t = get_list_token(&paren);
			}

			dsattribute_release(a);

			t = pop_token();
			if (t->type != TokenSemi)
			{
				fprintf(stderr, "Expecting semicolon \";\" at line %d\n", line_number);
				freeToken(t);
				dsrecord_release(r);
				return NULL;
			}
			freeToken(t);
		}
		else if (t->type == TokenCHILDREN)
		{
			t = pop_token();
			if (t->type != TokenEqual)
			{
				fprintf(stderr, "Expecting equal \"=\" at line %d\n", line_number);
				freeToken(t);
				dsrecord_release(r);
				return NULL;
			}
			freeToken(t);

			t = pop_token();

			paren = 0;
			if (t->type == TokenLeftParen)
			{
				paren = 1;
				freeToken(t);
			}
			else push_token(t);
				
			forever
			{
				k = get_dir(level + 1);
				if (k != NULL)
				{
					k->super = r->dsid;
					store_record(k);
					dsrecord_append_sub(r, k->dsid);
					dsrecord_release(k);
				}

				t = pop_token();
				if (t->type == TokenComma)
				{
					freeToken(t);
					continue;
				}

				x = 0;
				if (t->type == TokenRightParen)
				{
					x = 1;
					freeToken(t);
					if (paren == 0)
					{
						fprintf(stderr, "Unexpected right paren \")\" at line %d\n", line_number);
						dsrecord_release(r);
						return NULL;
					}
					paren = 0;
					t = pop_token();
				}

				if (t->type == TokenSemi)
				{
					x = 1;
					freeToken(t);
					if (paren == 1)
					{
						fprintf(stderr, "Expecting right paren \")\" at line %d\n", line_number);
						dsrecord_release(r);
						return NULL;
					}
				}
				
				if (x == 1) break;
			}
		}
		else if ((level > 0) && ((t->type == TokenComma) || (t->type == TokenRightParen)))
		{
			push_token(t);
			return r;
		}
		else if ((level == 0) && t->type == TokenNULL)
		{
			return r;
		}
		else
		{
			fprintf(stderr, "Unexpected token \"%s\" at line %d\n", token_string(t), line_number);
			freeToken(t);
			dsrecord_release(r);
			return NULL;
		}
	}
	
	dsrecord_release(r);
	return NULL;
}

ni_status
destroy_children(void *domain, ni_id *dir)
{
	ni_status status;
	int i;
	ni_idlist children;
	ni_id child;

	/* get a list of all my children */
	NI_INIT(&children);
	status = ni_children(domain, dir, &children);
	if (status != NI_OK) return status;

	/* destroy each child */
	for (i = 0; i < children.ni_idlist_len; i++)
	{
		child.nii_object = children.ni_idlist_val[i];
		status = ni_self(domain, &child);
		if (status != NI_OK) return status;
		status = ni2_destroydir(domain, &child, dir);
		if (status != NI_OK) return status;
	}

	/* free list of child ids */
	ni_idlist_free(&children);

	return NI_OK;
}

ni_proplist *
dsrecord_to_proplist(dsrecord *r)
{
	ni_proplist *pl;
	int i, j;
	ni_property p;

	if (r == NULL) return NULL;

	pl = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(pl);

	for (i = 0; i < r->count; i++)
	{
		NI_INIT(&p);
		p.nip_name = ni_name_dup(r->attribute[i]->key->data);

		for (j = 0; j < r->attribute[i]->count; j++)
		{
			ni_namelist_insert(&p.nip_val, r->attribute[i]->value[j]->data, NI_INDEX_NULL);
		}

		ni_proplist_insert(pl, p, NI_INDEX_NULL);
		ni_prop_free(&p);
	}

	return pl;	
}

int
load_record(dsrecord *r, void *domain, ni_id *dir, int level)
{
	int i, x;
	dsrecord *k;
	ni_proplist *pl;
	ni_id child_dir;
	ni_status status;

	if (r == NULL) return 0;
	
	/* write input proplist to netinfo */
	pl = dsrecord_to_proplist(r);

	if (level == 0)
		status = ni_write(domain, dir, *pl);
	else
		status = ni_create(domain, dir, *pl, &child_dir, NI_INDEX_NULL);

	ni_proplist_free(pl);
	if (status != NI_OK)
	{
		fprintf(stderr, "Can't %s directory: %s\n", ((level == 0) ? "write" : "create"), ni_error(status));
		return -1;
	}

	if (level == 0)
	{
		status = destroy_children(domain, dir);
		if (status != NI_OK)
		{
			fprintf(stderr, "Can't destroy child directories: %s\n", ni_error(status));
			return -1;
		}
	}

	for (i = 0; i < r->sub_count; i++)
	{
		k = fetch_record(r->sub[i]);
		x = 0;
		if (level == 0) x = load_record(k, domain, dir, level + 1);
		else x = load_record(k, domain, &child_dir, level + 1);
		dsrecord_release(k);
		if (x != 0) return x;
	}

	return 0;
}

void
raw_load(void *domain, char *path, char *file)
{
	dsrecord *root;

	ni_status status;
	ni_id nidir;

	if (path[0] != '/')
	{
		fprintf(stderr, "Raw load requires an absolute directory pathname, found %s\n", path);
		return;
	}

	status = ni_pathsearch(domain, &nidir, path);
	if (status == NI_NODIR)
	{
		status = ni2_createpath(domain, &nidir, path);
		if (status != NI_OK)
		{
			fprintf(stderr, "Can't create directory %s: %s\n", path, ni_error(status));
			return;
		}
	}

	if (status != NI_OK)
	{
		fprintf(stderr, "Can't access directory %s: %s\n", path, ni_error(status));
		return;
	}


	fp = stdin;
	if (file != NULL)
	{
		fp = fopen(file, "r");
		if (fp == NULL)
		{
			perror(file);
			return;
		}
	}

	root = get_dir(0);
	load_record(root, domain, &nidir, 0);
	dsrecord_release(root);

	store_clean();
}
