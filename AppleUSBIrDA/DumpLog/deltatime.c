// quick hack to re-compute the delta times in an irdalog

#include <stdio.h>

#define Reset 0
#define Delta 1

int state = Reset;
long last_fract, last_secs;
char buf[1000];

int main(int argc, char *argv[])
{
    long secs;                      /* the seconds */
    long fract;                     /* the fraction */
    int rc1, rc2;
    long dt1, dt2;
    char *ts, *tf;                  /* start of seconds string and fraction string */
    char *index(), *rindex();

    while (fgets(buf, sizeof(buf), stdin) != NULL) {        /* loop over input */
	tf = index(buf, '.');           /* find the decimal */
	if (tf == NULL) {               /* if none, blow off this line */
	    state = Reset;
	    printf("%s", buf);
	    continue;
	}
	*tf = 0;                        /* zap the period */
	ts = rindex(buf, ' ');          /* look for start of seconds */
	if (ts == NULL) ts = buf;
	else ts++;
	*tf = '.';                      /* put back the period */
	tf++;
	rc1 = sscanf(ts, "%ld", &secs);
	rc2 = sscanf(tf, "%ld", &fract);

	if (rc1 == 1 && rc2 == 1) {     /* parsed it ok */
	    if (state == Reset) {
		 printf("%s", buf);
		 state = Delta;
	    }
	    else {
		 dt2 = fract - last_fract;
		 dt1 = secs - last_secs;
		 if (dt2  < 0) {
		    dt2  += 1000000;
		    dt1--;
		 }
		 if (dt1 > 99) dt1 = 99;    // seconds overflow
		 if (1) {               // splice in the new delta time (bigger)
		    char *b1, *b2;
		    b1 = index(buf, '[');
		    b2 = index(buf, ']');
		    if (b1 && b2) {
			b1++;           // end after the [
			*b1 = 0;
			if (dt1 > 0)
			    printf("%s%2ld.%06ld%s", buf, dt1, dt2, b2);
			else
			    printf("%s   %6ld%s", buf,        dt2, b2);
		    }
		    else
			printf("%2ld.%06ld %s", dt1, dt2, buf);
		 }
	    }
	    last_secs = secs;
	    last_fract = fract;
	    continue;
	}
	// if verbose ...
	//fprintf(stderr, "%s: %s", "Parse Error", buf);
    }
    return 0;
}
