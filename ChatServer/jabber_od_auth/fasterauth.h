/*
 *  fasterauth.h
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#ifndef __FASTER_AUTH_H__
#define __FASTER_AUTH_H__

int FasterAuthentication(char *nodename, char *user,
                         char *challenge, char *response,
                         char **serverresponse);

#endif
