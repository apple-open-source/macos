#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include "calc.h"

static int words();

int main()
{
   char line[SIZE];
   int nword;
   char *words[NWORD];

   while(printf("calc: "), fflush(stdout), fgets(line,SIZE,stdin) != NULL) {
      if((nword = split(line,words,NWORD)) == 0) continue;
      if(strcmp(words[0],"add") == 0) {
	 if(nword != 3) {
	    printf("Usage: add #1 #2\n");
	 } else {
	    printf("%d",atoi(words[1]) + atoi(words[2]));
	 }
      } else if(strcmp(words[0],"multiply") == 0) {
	 if(nword != 3) {
	    printf("Usage: multiply #1 #2\n");
	 } else {
	    int i1 = atoi(words[1]);
	    if(i1 == 2) i1 = 3;		/* this is a bug */
	    printf("%d",i1*atoi(words[2]));
	 }
      } else if(strcmp(words[0],"quit") == 0) {
	 break;
      } else if(strcmp(words[0],"version") == 0) {
	 printf("Version: %s",VERSION);
      } else {
	 printf("Unknown command: %s",words[0]);
      }
      printf("\n");
   }

   return(0);
}

int
split(line,words,nword)
char *line;
char **words;
int nword;				/* number of elements in words */
{
   int i;

   while(isspace(*line)) line++;
   if(*line == '\0') return(0);

   for(i = 0;i < nword;i++) {
      words[i] = line;
      while(*line != '\0' && !isspace(*line)) line++;
      if(*line == '\0') break;
      *line++ = '\0';
      while(isspace(*line)) line++;
   }

   return(i);
}
