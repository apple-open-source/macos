/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */



#include <stdio.h>
#include <stdlib.h>


#include "queue_data.h"

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int new_queue_data(struct queue_data *qd, u_short size)
{

    qd->data = malloc(size);
    if (!qd->data)
        return 1;

    qd->maxsize = size;
    qd->first = 0;
    qd->curlen = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void free_queue_data(struct queue_data *qd)
{
    if (qd->data) {
        free(qd->data);
        qd->data = 0;
    }

    qd->maxsize = 0;
    qd->first = 0;
    qd->curlen = 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void enqueue_data(struct queue_data *qd, u_char *data, u_short len)
{
    u_short i;

    for (i = 0; i < len; i++) {
        if (qd->curlen < qd->maxsize) {
            qd->data[(qd->first + qd->curlen) % qd->maxsize] = data[i];
            qd->curlen++;
        }
        else {
            qd->data[qd->first] = data[i];
            qd->first = (qd->first + 1) % qd->maxsize;
        }
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void dequeue_data(struct queue_data *qd, u_char *data, u_short *len)
{
    u_short i;

    if (*len > qd->curlen)
        *len = qd->curlen ;

    for (i = 0; i < *len; i++) {
        data[i] = qd->data[qd->first];
        qd->first = (qd->first + 1) % qd->maxsize;
        qd->curlen--;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void peekqueue_data(struct queue_data *qd, u_char *data, u_short *len, u_short start)
{
    u_short i;

    if (*len > (qd->curlen - start))
        *len = qd->curlen ;

    for (i = 0; i < *len; i++) {
        data[i] = qd->data[(qd->first + i) % qd->maxsize];
    }
}

