#ifndef si_cms_hash_agility_data_h
#define si_cms_hash_agility_data_h

#include <stdio.h>
#include <stdint.h>

/* Random data for content */
extern unsigned char content[1024];
extern size_t  content_size;

/* Random data for hash agility attribute */
extern unsigned char attribute[32];

/* Random data for hash agility V2 attribute */
extern unsigned char _attributev2[64];

/* Valid CMS message on content with hash agility attribute */
extern uint8_t valid_message[];
extern size_t valid_message_size;
/*
 * Invalid CMS message on content with hash agility attribute.
 * Only the hash agility attribute value has been changed from the valid message.
 */
extern uint8_t invalid_message[];
extern size_t invalid_message_size;

/* Valid CMS message with no hash agility attribute */
extern unsigned char valid_no_attr[];
extern size_t valid_no_attr_size;

#include "si-cms-signing-identity-p12.h"

extern unsigned char _V2_valid_message[];
extern size_t _V2_valid_message_size;

#endif /* si_cms_hash_agility_data_h */
