/* MakeArgv.c
 *
 * Purpose: Translate a command line string into an array of arguments
 *   for use with the user commands;  Pre-process a command line string
 *   and expand $variables for the macros.
 */

#include "Sys.h"
#include <ctype.h>
#include "Util.h"
#include "MakeArgv.h"

static char *gMAVErrStrings[] = {
	NULL,
	"Unbalanced quotes",
	"Too many sets of quotes",
	"Cannot redirect output to more than one file",
	"No filename supplied to redirect output to",
	"No command supplied to pipe output to",
	"Redirection of input not implemented",
	"Cannot redirect and pipe out at the same time"
};

int MakeArgVector(char *str, CmdLineInfoPtr clp)
{
	register char *src;		/* The command line to parse. */
	register char *dst;		/* Where the argument(s) are written. */
	int argc;				/* The number of arguments. */
	char **argv;			/* An array of pointers to the arguments. */
	char quotestack[kMaxQuotePairs + 1];
							/* Keeps track of each successive group of
							 * (different) quotes.
							 */
	char *qp;				/* The most recent set of quotes we're tracking. */
	int err;				/* Which syntax error occurred. */
	int addQuoteToArg;		/* Needed to tell whether a quote character
							 * should be added to the current argument.
							 */
	int prevArgType;		/* Used to track if a previous parameter was
							 * one that should not be included in the
							 * argument buffer, such as a redirection
							 * symbol (>, >>) and its filename.
							 */
	char *curArg;			/* Points to the beginning of the token we
							 * are writing to.
							 */
	unsigned int c;			/* Char to copy to dst. */
	char numBuf[4];			/* An atoi buffer for a 3-digit octal number. */
	unsigned int octalNum;	/* The resulting integer. */

	err = kMavNoErr;
	src = str;
	dst = clp->argBuf;
	argc = 0;
	argv = clp->argVector;
	
	/* Initialize the stack of quote characters.  Each time we get a
	 * quote character, we add it to the stack.  When we encounter
	 * another quote character of the same type, we remove it from the
	 * stack to denote that the set has been balanced.
	 *
	 * The stack pointer, qp, is init'ed to the first element of the
	 * stack, to denote that there are no quote sets active.
	 */
	qp = quotestack;
	*qp = 0;

	/* Init this to a regular type, meaning we don't have to do any
	 * cleanup after this argument like removing the redirected file
	 * from the list of arguments.
	 */
	prevArgType = kRegularArg;
	
	/* Initially, no outfile has been specified. */
	*clp->outFileName = 0;
	clp->isAppend = 0;
	
	/* Initially, no pipe sub-command either. */
	*clp->pipeCmdLine = 0;

	for (;argc < kMaxArgs; argc++) {
		/* First, decide where to write the next argument.
		 * Normally we would write it to the argument buffer,
		 * but if in a previous iteration we detected a redirection
		 * symbol, we would want to write the next argument to the
		 * outfile string instead of adding the name of the out file
		 * to the argument list.
		 */
		if (prevArgType == kRegularArg)
			argv[argc] = dst;
		else {
			/* If the out file string already had something in it when
			 * we get here, it means that we are ready to start collecting
			 * arguments again.
			 */
			if (*clp->outFileName) {
				/* Fix the argument counter to what it should be. */
				argc -= 2;
				
				/* Re-set dst to point to the argument directly after
				 * the argument before the redirection symbol and the
				 * redirected filename.
				 */
				dst = argv[argc];
				
				/* Don't forget to keep track of whether we got an
				 * overwrite redirection symbol (>) or an append
				 * symbol (>>).
				 */
				clp->isAppend = (prevArgType == kReDirectOutAppendArg);
				
				/* Tell the parser that we're writing arguments again. */
				prevArgType = kRegularArg;
			} else {
				/* The out file's name has not been set yet, so write the
				 * next token as the name of the out file.
				 */
				dst = clp->outFileName;
			}
		}
		
		/* Save the location of the current token for later. */
		curArg = dst;
		*curArg = 0;

		/* Skip any whitespace between arguments. */
		while (isspace(*src))
			src++;
		if (!*src) {
			/* We have reached the end of the command line. */
			
			/* If dst was left pointing to the name of the outfile,
			 * outfile has an empty name, meaning the user did something
			 * like 'aaa bbb >' and forgot the outfile name.
			 */
			if (dst == clp->outFileName) {
				err = kMavErrNoReDirectedFileName;
				argc--;
			}
				
			/* If the quote stack pointer does not point to the
			 * first element of the stack, it means that there
			 * is atleast one set of quotes that wasn't properly
			 * balanced.
			 */
			if (qp != quotestack)
				err = kMavErrUnbalancedQuotes;
			goto done;
		}

		for (;;) {
			if (*src == '\\') {
				/* As usual, go ahead and add the character directly after
				 * backslash without worrying about that character's
				 * special meaning, if any.
				 */
				src++;	/* Skip the backslash, which we don't want. */
				if (isdigit(src[0]) && isdigit(src[1]) && isdigit(src[2])) {
					/* They can also specify a non-printing character,
					 * by saying \xxx.
					 */
					memcpy(numBuf, src, (size_t)3);
					numBuf[3] = '\0';
					sscanf(numBuf, "%o", &octalNum);
					c = octalNum;
					src += 3;
					goto copyCharToArg2;
				}
				goto copyCharToArg;
			} else if (!*src) {
				/* If there are no more characters in the command line,
				 * end the current argument in progress.
				 */
				*dst++ = '\0';
				break;
			} else if (*src == '^') {
				/* Let them fake a control character by using the two
				 * character sequence ^X.
				 */
				++src;
				if (*src == '?')
					c = 0x7f;	/* DEL */
				else
					c = ((*src) & 31);
				++src;
				goto copyCharToArg2;
			} else if (ISQUOTE(*src)) {
				/* First, check to see if this quote is the second
				 * quote to complete the pair.
				 */
				if (*src == *qp) {
					/* If it is, remove this set from the stack. */
					*qp-- = 0;
					
					/* Add the quote to the argument if it is not
					 * the outermost set.
					 */
					addQuoteToArg = (quotestack < qp);
				} else {
					/* Add the quote to the argument if it is not
					 * the outermost set.
					 */
					addQuoteToArg = (quotestack < qp);
					
					/* It's extremely unlikely, but prevent over-writing
					 * the quotestack if the user has an extremely
					 * complicated command line.
					 */
					if (qp < (quotestack + kMaxQuotePairs))
						*++qp = *src;
					else {
						err = kMavErrTooManyQuotePairs;
						goto done;
					}
				}
				if (addQuoteToArg)
					goto copyCharToArg;
				else
					++src;
			} else if (qp == quotestack) {
				/* Take action on some special characters, as long
				 * as we aren't in the middle of a quoted argument.
				 */
				if (isspace(*src)) {
					/* End an argument in progress if we encounter
					 * delimiting whitespace, unless this whitespace
					 * is being enclosed within quotes.
					 */
					*dst++ = 0;
					break;
				} else if (*src == '>') {
					/* The user wants to redirect stdout out to a file. */
					if (*curArg != 0) {
						/* End an argument in progress.  We'll be back
						 * at the statement above in the next loop
						 * iteration, but we'll take the 'else'
						 * branch next time since it will look like we
						 * had an empy argument.
						 */
						*dst++ = 0;	
					} else {
						/* Make sure the user doesn't try to redirect
						 * to 2 different files.
						 */
						if ((*clp->outFileName == 0) && (prevArgType == kRegularArg)) {
							++src;	/* Skip the redirection symbol (>). */
							prevArgType = kReDirectOutArg;
							if (*src == '>') {
								/* If there we had >>, we want to append
								 * to a file, not overwrite it.
								 */
								++src;	/* Skip this one also. */
								prevArgType = kReDirectOutAppendArg;
							}
						} else {
							err = kMavErrTooManyReDirections;
							goto done;
						}
					}
					break;
					/* End '>' */
				} else if (*src == '<') {
					err = kMavErrNoReDirectedInput;
					goto done;
					/* End '<' */
				} else if (*src == '|') {
					/* The rest of this line is an another whole
					 * sub-command line, so we copy everything beyond
					 * the | into the string pipeCmd.
					 */

					/* But first check if we had started an argument,
					 * and if we did, go ahead and
					 * finish it.
					 */
					if (dst > curArg)
						*dst++ = 0;
					else {
						/* We have an empty argument, so get rid of it.
						 * To do that, we prevent the arg counter from
						 * advancing to the next item by decrementing
						 * it now, and letting the loop counter
						 * re-increment it, thus having the effect of
						 * not adding anything at all.
						 */
						--argc;
					}

					/* It would be an error to try and >redirect and
					 * |pipe out both, so check for this first by
					 * making sure the outfile is empty.
					 */
					if (*clp->outFileName != 0) {
						err = kMavErrBothPipeAndReDirOut;
						goto done;
					}

					/* Now start writing to the pipe string. */
					dst = clp->pipeCmdLine;
					src++;		/* Don't copy the | character... */
					while (isspace(*src))	/* ...or whitespace. */
						src++;
					while ((*src != '\0') && (*src != '\n'))
						*dst++ = *src++;
					*dst = 0;
					
					/* Ensure the user gave us something to pipe into. */
					if (dst == clp->pipeCmdLine)
						err = kMavErrNoPipeCommand;
					break;
					/* End '|' */
				} else if (*src == '#') {
					/* Ignore everything after the #comment character. */
					*src = 0;
					
					/* If we had started an argument, go ahead and
					 * finish it.
					 */
					if (dst > curArg)
						*dst++ = 0;
					else {
						/* We have an empty argument, so get rid of it.
						 * To do that, we prevent the arg counter from
						 * advancing to the next item by decrementing
						 * it now, and letting the loop counter
						 * re-increment it, thus having the effect of
						 * not adding anything at all.
						 */
						--argc;
					}
					break;
					/* End '#' */
				} else if (*src == '!') {
					/* We need a special case when the user want to
					 * spawn a shell command.  We need argument 0 to
					 * be just an !.
					 */
					if (dst == argv[0]) {
						/* Form argument one, then break. */
						*dst++ = *src++;
						*dst++ = 0;
						break;
					} else {
						/* Otherwise pretend that ! is a normal character. */
						goto copyCharToArg;
					}
					/* End '!' */
				} else {
					/* We have a regular character, not within a
					 * quoted string, to copy to an argument.
					 */
					goto copyCharToArg;
				}
				/* End if not within a quoted string. */
			} else {
				/* If we get to this point (naturally), we are within
				 * set of quotes, so just copy everything in between
				 * them to the argument.
				 */
				 
copyCharToArg:	/* Add this character to the current token. */
				c = *src++;
copyCharToArg2:
				*dst++ = c;
			}
			/* End collecting an argument. */
		}
		/* End collecting all arguments. */
	}

done:

	/* Probably should not act on lines with syntax errors. */
	if (err)
		argc = 0;
	clp->argCount = argc;
	argv[argc] = NULL;
	
	/* Keep a copy of the original command line, for commands that
	 * need to peek at it.
	 */
	argv[argc + 1] = str;
	argv[argc + 2] = (char *) clp;
	argv[argc + 3] = NULL;	/* This will stay NULL. */

	clp->err = err;
	clp->errStr = gMAVErrStrings[err];
	return err;
}	/* MakeArgVector */




/* This routine scans a command line and expands dollar-variables, like
 * $2 (to denote the second argument), given a previously formed argument
 * vector.  Why isn't this a part of the MakeArgVector() routine, above?
 * Because MakeArgVector() quits after it finds a |;  Because we only
 * need to do this for macros, so don't waste time doing it if it isn't
 * necessary;  and because MakeArgVector() is already quite large.
 */
void ExpandDollarVariables(char *cmdLine, size_t sz, int argc, char **argv)
{
	char *dst, *cp, *cp2, *start;
	char s[32];
	int argNum, argNum2;
	size_t dstLen, amtToMove;
	long amtToShift;

	/* We need to create a buffer large enough to hold a possibly long
	 * string consisting of several arguments cat'ted together.
	 */
	dst = (char *) malloc(sz);
	if ((dst != NULL) && (argc > 0)) {
		/* Scan the line looking for trouble (i.e. $vars). */
		for (cp = cmdLine; ; cp++) {
			/* Allow the user to insert a $ if she needs to, without
			 * having our routine trying to expand it into something.
			 */
			if (*cp == '\\') {
				++cp;	/* skip backslash. */
				if (!*cp)
					break;	/* no \, then 0 please. */
				++cp;	/* skip thing being escaped. */
			}
			if (!*cp)
				break;	/* Done. */
			if (*cp == '$') {
				start = cp;
				*dst = 0;
				/* Now determine whether it is the simple form,
				 * like $3, or a complex form, which requires parentheses,
				 * such as:
				 *   $(1,2,4)  :  Insert arguments one, two, and four.
				 *   $(1-4)    :  Arguments one through four inclusive.
				 *   $(3+)     :  Arg 3, and all args after it.
				 * or a special form, such as:
				 *   $*        :  All arguments (except 0).
				 *   $@        :  All arguments (except 0), each "quoted."
				 */
				++cp;	/* skip $. */
				if (*cp == '(') {
					/* Complex form. */
					++cp;	/* Skip left paren. */
					
					while (1) {
						if (!*cp) break;
						if (*cp == ')') {
							++cp;
							break;
						}
						/* Collect the first (and maybe only) number. */
						for (cp2=s; isdigit(*cp); )
							*cp2++ = *cp++;
						*cp2 = 0;
						argNum = atoi(s);
						if (argNum >= 0 && argNum < argc) {
							if (!*dst)
								strncpy(dst, argv[argNum], sz);
							else {
								strncat(dst, " ", sz);							
								strncat(dst, argv[argNum], sz);							
							}
							if (*cp == '-') {
								/* We have a range. */
								++cp;	/* Skip dash. */
								/* Collect the second number. */
								for (cp2=s; isdigit(*cp); )
									*cp2++ = *cp++;
								*cp2 = 0;
								argNum2 = atoi(s);
								
								/* Make sure we don't copy too many. */
								if (argNum2 >= argc)
									argNum2 = argc - 1;
								if (argNum2 > argNum)
									goto copyRest;
								/* End range. */
							} else if (*cp == '+') {
								++cp;
								argNum2 = argc - 1;
							copyRest:
								for (++argNum; argNum <= argNum2; ++argNum) {
									strncat(dst, " ", sz);
									strncat(dst, argv[argNum], sz);
								}
							}
							/* End if the argument number was valid. */
						}
						/* Now see if we have a comma.  If we have a comma,
						 * we can do some more.  Otherwise it is either a
						 * bad char or a right paren, so we'll break out.
						 */
						if (*cp == ',')
							++cp;
						/* End loop between parens. */
					}
				} else if (isdigit(*cp)) {
					/* Simple form. */
					for (cp2=s; isdigit(*cp); )
						*cp2++ = *cp++;
					*cp2 = 0;
					argNum = atoi(s);
					if (argNum >= 0 && argNum < argc)
						strncpy(dst, argv[argNum], sz);
				} else if (*cp == '*') {
					/* Insert all arguments, except 0. */
					++cp;	/* Skip *. */
					argNum = 1;
					argNum2 = argc - 1;
					if (argNum <= argNum2)
						strncpy(dst, argv[argNum], sz);
					for (++argNum; argNum <= argNum2; ++argNum) {
						strncat(dst, " ", sz);
						strncat(dst, argv[argNum], sz);
					}
				} else if (*cp == '@') {
					/* Insert all arguments, except 0, each quoted. */
					++cp;	/* Skip @. */
					argNum = 1;
					argNum2 = argc - 1;
					if (argNum <= argNum2) {
						strncpy(dst, "\"", sz);
						strncat(dst, argv[argNum], sz);
						strncat(dst, "\"", sz);
					}
					for (++argNum; argNum <= argNum2; ++argNum) {
						strncat(dst, " \"", sz);
						strncat(dst, argv[argNum], sz);
						strncat(dst, "\"", sz);
					}
				}
				dstLen = strlen(dst);
				
				/* This is the number of bytes we need to move the
				 * rest of the line down, so we can insert the
				 * variable's value, which would be the dst pointer.
				 */
				amtToShift = dstLen - (cp - start);
				
				/* Don't bother moving if we don't have to. */
				if (amtToShift != 0) {
					amtToMove = (size_t) (
						(long)sz - (long)(cp - cmdLine) - amtToShift - 1);
					/* Move the rest of the line down, so we can do
					 * the insertion without overwriting any of the
					 * original string.
					 */
					MEMMOVE(cp + amtToShift, cp, amtToMove);
				}
				
				/* Copy the value of the variable into the space we
				 * created for it.
				 */
				memcpy(start, dst, dstLen);
				
				/* Adjust the scan pointer so it points after the stuff
				 * we just wrote.
				 */
				cp = start + amtToShift;
			}
			
		}
		/* It's possible (but extremely unlikely) that the null terminator
		 * got overwritten when we did an insertion.  This could happen
		 * if the command line was very close to "full," or if the stuff
		 * we inserted was long.
		 */
		cmdLine[sz - 1] = 0;

		free(dst);
	}
}	/* ExpandDollarVariables */

/* eof makeargv.c */
