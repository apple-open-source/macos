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



struct queue_data {
    u_char 	*data;
    u_short 	first;
    u_short	curlen;
    u_short 	maxsize;
};



int new_queue_data(struct queue_data *qd, u_short size);
void free_queue_data(struct queue_data *qd);
void enqueue_data(struct queue_data *qd, u_char *data, u_short len);
void dequeue_data(struct queue_data *qd, u_char *data, u_short *len);
void peekqueue_data(struct queue_data *qd, u_char *data, u_short *len, u_short start);

