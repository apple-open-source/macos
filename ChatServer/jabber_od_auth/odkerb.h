/*
 *  odkerb.h
 *
 *  obtain IM handle using DS/OD
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#ifndef __ODKERB_H__
#define __ODKERB_H__

#define kIMTypeJABBER "JABBER:"

int odkerb_get_im_handle(char *service_principal_id, char *realm, char *im_type, char im_handle[], size_t im_handle_size);

#endif
