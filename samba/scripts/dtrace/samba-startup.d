#!/usr/sbin/dtrace -C -s

/* Copyright (C) 2007 Apple Inc. All Rights Reserved. */

#pragma D option flowindent

#define NSEC_TO_USEC(nsec) ((nsec) / 1000)
#define NSEC_TO_MSEC(nsec) ((nsec) / 1000000)
#define NSEC_TO_SEC(nsec) ((nsec) / 1000000000)

#define BASE_STACK_DEPTH (5)

BEGIN { printf("tracing startup for process $1 is %d\n", $1); }

pid$1:smbd::entry
/ ustackdepth == BASE_STACK_DEPTH /
{
    self->func = probefunc;
    self->start[ustackdepth] = timestamp; /* nanoseconds */
    trace();
}

pid$1:smbd::return
/ ustackdepth == (BASE_STACK_DEPTH - 1) && probefunc == self->func/
{
    /* We need to tweak the stackdepth because we end up here
     * AFTER the matching function return.
     */
    this->elapsed = timestamp - self->start[ustackdepth + 1];
    printf("%d.%d sec elapsed\n",
	    NSEC_TO_SEC(this->elapsed), NSEC_TO_MSEC(this->elapsed));

    self->func = 0;
    @fcost[probefunc] = sum(NSEC_TO_USEC(this->elapsed));
}

END
{
    printa(@fcost);
}
