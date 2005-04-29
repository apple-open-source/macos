/*
 * libxslt_pipes.c: a program for performing a series of XSLT
 * transformations
 *
 * Writen by Panos Louridas, based on libxslt_tutorial.c by John Fleck.
 *
 * Copyright (c) 2004 Panagiotis Louridas
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions: </para>
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */ 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>

#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

extern int xmlLoadExtDtdDefaultValue;

static void usage(const char *name) {
    printf("Usage: %s [options] stylesheet [stylesheet ...] file [file ...]\n",
            name);
    printf("      --out file: send output to file\n");
    printf("      --param name value: pass a (parameter,value) pair\n");
}

int main(int argc, char **argv) {
    int arg_indx;
    const char *params[16 + 1];
    int params_indx = 0;
    int stylesheet_indx = 0;
    int file_indx = 0;
    int i, j, k;
    FILE *output_file = stdout;
    xsltStylesheetPtr *stylesheets = 
        (xsltStylesheetPtr *) calloc(argc, sizeof(xsltStylesheetPtr));
    xmlDocPtr *files = (xmlDocPtr *) calloc(argc, sizeof(xmlDocPtr));
    xmlDocPtr doc, res;
    int return_value = 0;
        
    if (argc <= 1) {
        usage(argv[0]);
        return_value = 1;
        goto finish;
    }
        
    /* Collect arguments */
    for (arg_indx = 1; arg_indx < argc; arg_indx++) {
        if (argv[arg_indx][0] != '-')
            break;
        if ((!strcmp(argv[arg_indx], "-param"))
                || (!strcmp(argv[arg_indx], "--param"))) {
            arg_indx++;
            params[params_indx++] = argv[arg_indx++];
            params[params_indx++] = argv[arg_indx];
            if (params_indx >= 16) {
                fprintf(stderr, "too many params\n");
                return_value = 1;
                goto finish;
            }
        }  else if ((!strcmp(argv[arg_indx], "-o"))
                || (!strcmp(argv[arg_indx], "--out"))) {
            arg_indx++;
            output_file = fopen(argv[arg_indx], "w");
        } else {
            fprintf(stderr, "Unknown option %s\n", argv[arg_indx]);
            usage(argv[0]);
            return_value = 1;
            goto finish;
        }
    }
    params[params_indx] = 0;

    /* Collect and parse stylesheets and files to be transformed */
    for (; arg_indx < argc; arg_indx++) {
        char *argument =
            (char *) malloc(sizeof(char) * (strlen(argv[arg_indx]) + 1));
        strcpy(argument, argv[arg_indx]);
        if (strtok(argument, ".")) {
            char *suffix = strtok(0, ".");
            if (suffix && !strcmp(suffix, "xsl")) {
                stylesheets[stylesheet_indx++] =
                    xsltParseStylesheetFile((const xmlChar *)argv[arg_indx]);;
            } else {
                files[file_indx++] = xmlParseFile(argv[arg_indx]);
            }
        } else {
            files[file_indx++] = xmlParseFile(argv[arg_indx]);
        }
        free(argument);
    }

    xmlSubstituteEntitiesDefault(1);
    xmlLoadExtDtdDefaultValue = 1;

    /* Process files */
    for (i = 0; files[i]; i++) {
        doc = files[i];
        res = doc;
        for (j = 0; stylesheets[j]; j++) {
            res = xsltApplyStylesheet(stylesheets[j], doc, params);
            xmlFreeDoc(doc);
            doc = res;
        }

        if (stylesheets[0]) {
            xsltSaveResultToFile(output_file, res, stylesheets[j-1]);
        } else {
            xmlDocDump(output_file, res);
        }
        xmlFreeDoc(res);
    }

    fclose(output_file);

    for (k = 0; stylesheets[k]; k++) {
        xsltFreeStylesheet(stylesheets[k]);
    }

    xsltCleanupGlobals();
    xmlCleanupParser();

 finish:
    free(stylesheets);
    free(files);
    return(return_value);
}
