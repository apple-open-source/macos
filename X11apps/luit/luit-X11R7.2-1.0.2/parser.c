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
/* $XFree86$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"

static char keyword[MAX_KEYWORD_LENGTH];

static void
skipEndOfLine(FILE *f, int c)
{
    if(c == 0)
        c = getc(f);
    
    for(;;)
        if(c <= 0 || c == '\n')
            return;
        else
            c = getc(f);
}

static int
drainWhitespace(FILE *f, int c)
{
    if(c == 0)
        c = getc(f);

    while (c == '#' || c == ' ' || c == '\t') {
        if(c <= 0)
            return 0;
        if(c == '#') {
            skipEndOfLine(f, c);
            return '\n';
        }
        c = getc(f);
    }

    return c;
}

static int
getString(FILE *f, int string_end, int *c_return)
{
    int i = 0;
    int c;

    c = getc(f);
    while(c > 0) {
        if(c == string_end)
            break;
        if(c == '\\') {
            c = getc(f);
            if(c == '\n')
                continue;
        }
        keyword[i++] = c;
        if(i >= MAX_KEYWORD_LENGTH)
            return TOK_ERROR;
        c = getc(f);
    }

    if(c <= 0)
        return TOK_ERROR;
    keyword[i] = '\0';
    *c_return = c;
    return TOK_KEYWORD;
}

static int
getToken(FILE *f, int c, int parse_assignments, int *c_return)
{
    int i;
    c = drainWhitespace(f, c);

    if(c < 0)
        return TOK_EOF;
    if(c == '\n') {
        *c_return = 0;
        return TOK_EOL;
    }

    if(parse_assignments && c == '=') {
        *c_return = 0;
        return TOK_EQUALS;
    }

    if(c == '\'' || c == '"')
        return getString(f, c, c_return);

    i = 0;
    while(c > 0 && c != ' ' && c != '\t' && c != '\n') {
        if(c == '\\') {
            c = getc(f);
            if(c == '\n')
                continue;
        }
        keyword[i++] = c;
        if(i >= MAX_KEYWORD_LENGTH)
            return TOK_ERROR;
        c = getc(f);
        if(parse_assignments && c == '=')
            break;
    }

    *c_return = c<0?0:c;
    keyword[i] = '\0';
    return TOK_KEYWORD;
}


/* Can parse both the old and new formats for locale.alias */
static int
parseTwoTokenLine(FILE *f, char *first, char *second)
{
    int c = 0;
    int tok;

  again:
    
    tok = getToken(f, c, 0, &c);
    if(tok == TOK_EOF)
        return -1;
    else if(tok == TOK_EOL)
        goto again;
    else if(tok == TOK_KEYWORD) {
        int len = strlen(keyword);
        if(keyword[len - 1] == ':')
            keyword[len - 1] = '\0';
        strcpy(first, keyword);
    } else
        return -2;

    tok = getToken(f, c, 0, &c);
    if(tok == TOK_KEYWORD) {
        strcpy(second, keyword);
    } else
        return -2;

    tok = getToken(f, c, 0, &c);
    if(tok != TOK_EOL)
        return -2;

    return 0;
}

char *
resolveLocale(char *locale)
{
    FILE *f;
    char first[MAX_KEYWORD_LENGTH], second[MAX_KEYWORD_LENGTH];
    char *resolved = NULL;
    int rc;

    f = fopen(LOCALE_ALIAS_FILE, "r");
    if(f == NULL)
        goto bail;

    do {
        rc = parseTwoTokenLine(f, first, second);
        if(rc < -1)
            goto bail;
        if(!strcmp(first, locale)) {
            resolved = malloc(strlen(second) + 1);
            if(resolved == NULL)
                goto bail;
            strcpy(resolved, second);
            break;
        }
    } while(rc >= 0);

    if(resolved == NULL) {
        resolved = malloc(strlen(locale) + 1);
        if(resolved == NULL)
            goto bail;
        strcpy(resolved, locale);
    }

    fclose(f);

    return resolved;

  bail:
    if(f != NULL)
        fclose(f);
    if(resolved != NULL)
        free(resolved);
    return NULL;
}
