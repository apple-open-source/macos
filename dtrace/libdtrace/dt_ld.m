/*
 * Copyright (c) 2006-2008 Apple Computer, Inc.  All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Foundation/Foundation.h>
// This must be done *after* any references to Foundation.h!
#define uint_t  __Solaris_uint_t

#include <dt_impl.h>
#include <dt_provider.h>
#include <dt_string.h>
#include <dt_program.h>
#include "dt_dof_byteswap.h"

#include <mach/machine.h>

#include "arch.h"

#include <stdlib.h>
#include <errno.h>
#include <mach/vm_param.h>

#define dtrace_separator			"$"

// Why an encoding & decoding prefix? During compilation, the prefix may change...
#define dtrace_stability_encoding_prefix	"___dtrace_stability"
#define dtrace_stability_decoding_prefix	"___dtrace_stability"
#define dtrace_stability_version		"v1"

#define dtrace_typedefs_encoding_prefix		"___dtrace_typedefs"
#define dtrace_typedefs_decoding_prefix		"___dtrace_typedefs"
#define dtrace_typedefs_version			"v2"

#define dtrace_probe_encoding_prefix		"__dtrace_probe"
#define dtrace_probe_decoding_prefix		"___dtrace_probe"
#define dtrace_probe_version			"v1"

#define dtrace_isenabled_encoding_prefix	"__dtrace_isenabled"
#define dtrace_isenabled_decoding_prefix	"___dtrace_isenabled"
#define dtrace_isenabled_version		"v1"

static char* dt_ld_encode_string(char* string)
{
	size_t input_length = strlen(string);
	char* results = malloc(input_length * 2 + 1);
	int i;
	for (i=0; i<input_length; i++) {
		sprintf(&results[i*2],"%02x", (unsigned int)string[i]);
	}
	results[input_length*2] = 0;
	
	return results;
}

static NSString* dt_ld_encode_nsstring(NSString* string)
{
	char* results = dt_ld_encode_string((char*)[string UTF8String]);
	NSString* value = [NSString stringWithUTF8String:results];
	free(results);
	
	return value;
}

static char* dt_ld_decode_string(char* string)
{
	size_t input_length = strlen(string) / 2;
	unsigned char* results = malloc(input_length + 1);
	int i;
	for (i=0; i<input_length; i++) {
		unsigned int value;
		sscanf(&string[i*2],"%2x", &value);
		results[i] = (unsigned char)value;
	}
	results[input_length] = 0;
	
	return (char*)results;
}

static NSString* dt_ld_decode_nsstring(NSString* string)
{
	char* results = dt_ld_decode_string((char*)[string UTF8String]);
	NSString* value = [NSString stringWithUTF8String:results];
	free(results);
	
	return value;
}

#pragma mark -
#pragma mark stability encoding / decoding

char* dt_ld_encode_stability(char* provider_name, dt_provider_t *provider)
{
	// Stability info is encoded as (dtrace_stability_encoding_prefix)(providerName)(dtrace_stability_version)(stability_data)
	size_t bufsize = sizeof(dtrace_stability_encoding_prefix) +
	sizeof(dtrace_separator) +
	sizeof(dtrace_stability_version) +
	sizeof(dtrace_separator) +
	strlen(provider_name) +
	sizeof(dtrace_separator) +
	sizeof(dtrace_pattr_t) * 3 + // Each attr is 1 byte * an encoding size of 3 bytes.
	1; // NULL terminator
	
	char* buffer = malloc(bufsize); 
	
	snprintf(buffer, bufsize, "%s%s%s%s%s%s%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x",
		 dtrace_stability_encoding_prefix,
		 dtrace_separator,
		 provider_name,
		 dtrace_separator,
		 dtrace_stability_version,
		 dtrace_separator,
		 /* provider attributes */
		 provider->pv_desc.dtvd_attr.dtpa_provider.dtat_name,
		 provider->pv_desc.dtvd_attr.dtpa_provider.dtat_data,
		 provider->pv_desc.dtvd_attr.dtpa_provider.dtat_class,
		 /* module attributes */
		 provider->pv_desc.dtvd_attr.dtpa_mod.dtat_name,
		 provider->pv_desc.dtvd_attr.dtpa_mod.dtat_data,
		 provider->pv_desc.dtvd_attr.dtpa_mod.dtat_class,
		 /* function attributes */
		 provider->pv_desc.dtvd_attr.dtpa_func.dtat_name,
		 provider->pv_desc.dtvd_attr.dtpa_func.dtat_data,
		 provider->pv_desc.dtvd_attr.dtpa_func.dtat_class,
		 /* name attributes */
		 provider->pv_desc.dtvd_attr.dtpa_name.dtat_name,
		 provider->pv_desc.dtvd_attr.dtpa_name.dtat_data,
		 provider->pv_desc.dtvd_attr.dtpa_name.dtat_class,
		 /* args[] attributes */
		 provider->pv_desc.dtvd_attr.dtpa_args.dtat_name,
		 provider->pv_desc.dtvd_attr.dtpa_args.dtat_data,
		 provider->pv_desc.dtvd_attr.dtpa_args.dtat_class);
	
	return buffer;
}

char* dt_ld_decode_stability_v1_level(int level) {
	switch(level) {
	case DTRACE_STABILITY_INTERNAL: return "INTERNAL";
	case DTRACE_STABILITY_PRIVATE:  return "PRIVATE";
	case DTRACE_STABILITY_OBSOLETE: return "OBSOLETE";
	case DTRACE_STABILITY_EXTERNAL: return "EXTERNAL";
	case DTRACE_STABILITY_UNSTABLE: return "UNSTABLE";
	case DTRACE_STABILITY_EVOLVING: return "EVOLVING";
	case DTRACE_STABILITY_STABLE:   return "STABLE";
	case DTRACE_STABILITY_STANDARD: return "STANDARD";
	default: return "ERROR!";
	};	
}

char* dt_ld_decode_stability_v1_class(int class) {
	switch(class) {
	case DTRACE_CLASS_UNKNOWN: return "UNKNOWN";
	case DTRACE_CLASS_CPU: return "CPU";
	case DTRACE_CLASS_PLATFORM: return "PLATFORM";
	case DTRACE_CLASS_GROUP: return "GROUP";
	case DTRACE_CLASS_ISA: return "ISA";
	case DTRACE_CLASS_COMMON: return "COMMON";
	default: return "ERROR!";
	};	
}

NSString* dt_ld_decode_stability_v1(NSArray* elements)
{
	NSString* provider = [elements objectAtIndex:1];
	NSScanner* scanner = [NSScanner scannerWithString:[elements objectAtIndex:3]];
	[scanner setCharactersToBeSkipped:[NSCharacterSet characterSetWithCharactersInString:@"_"]];
	NSMutableString* stability = [NSMutableString string];
	
	int value;
	char* name = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	char* data = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	char* class = dt_ld_decode_stability_v1_class([scanner scanInt:&value] == YES ? value : 255);
	[stability appendFormat:@"#pragma D attributes %s/%s/%s provider %@ provider\n", name, data, class, provider];
	
	name = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	data = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	class = dt_ld_decode_stability_v1_class([scanner scanInt:&value] == YES ? value : 255);
	[stability appendFormat:@"#pragma D attributes %s/%s/%s provider %@ module\n", name, data, class, provider];
	
	name = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	data = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	class = dt_ld_decode_stability_v1_class([scanner scanInt:&value] == YES ? value : 255);
	[stability appendFormat:@"#pragma D attributes %s/%s/%s provider %@ function\n", name, data, class, provider];
	
	name = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	data = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	class = dt_ld_decode_stability_v1_class([scanner scanInt:&value] == YES ? value : 255);
	[stability appendFormat:@"#pragma D attributes %s/%s/%s provider %@ name\n", name, data, class, provider];
	
	name = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	data = dt_ld_decode_stability_v1_level([scanner scanInt:&value] == YES ? value : 255);
	class = dt_ld_decode_stability_v1_class([scanner scanInt:&value] == YES ? value : 255);
	[stability appendFormat:@"#pragma D attributes %s/%s/%s provider %@ args\n", name, data, class, provider];
	
	return stability;
}

NSString* dt_ld_decode_stability(NSString* encoding)
{
	NSArray* elements = [encoding componentsSeparatedByString:@dtrace_separator];
	
	NSString* version = [elements objectAtIndex:2];
	
	if ([version isEqualToString:@"v1"])
		return dt_ld_decode_stability_v1(elements);
	
	// Wow, no good way to handle error conditions here.
	return [NSString stringWithFormat:@"Unhandled stability encoding version %@", version];		
}

#pragma mark -
#pragma mark typedef encoding / decoding

// DTrace typedefs a small number of default types by default.
// Unfortunately, these are not base types, and so they are
// encoded as specialized types. This works fine until link
// time, when DTrace sees the encoding as an attempt to redefine
// an existing type, and fails the link. This method creates
// a dictionary of types to ignore when encoding.

static NSDictionary* encodingExclusionTypes = nil;

static NSDictionary* dt_ld_create_encoding_exclusion_types() {
	NSMutableDictionary* excludedTypes = [NSMutableDictionary dictionary];
	
	// First walk all 32 bit typedefs
	extern const dt_typedef_t _dtrace_typedefs_32[];
	const dt_typedef_t *iter = _dtrace_typedefs_32;
	for (; iter->dty_src != NULL; iter++) {
		NSString* type = [NSString stringWithUTF8String:iter->dty_dst];
		[excludedTypes setObject:type forKey:type];
	}

	// These are almost certainly the same, but just in case...
	// Walk all 64 bit typedefs
	extern const dt_typedef_t _dtrace_typedefs_64[];
	iter = _dtrace_typedefs_64;
	for (; iter->dty_src != NULL; iter++) {
		NSString* type = [NSString stringWithUTF8String:iter->dty_dst];
		[excludedTypes setObject:type forKey:type];
	}
	
	return excludedTypes;
}

//
// If the input type is a pointer, return the type pointed to.
// This method is recursive, it will walk back a chain of pointers
// until it reaches a non pointer type.
//
// If the original type is not a pointer, it is returned unchanged.

static ctf_id_t dt_ld_strip_pointers(ctf_file_t *file, ctf_id_t type) {
	if (ctf_type_kind(file, type) == CTF_K_POINTER) {
		return dt_ld_strip_pointers(file, ctf_type_reference(file, type));
	}
	
	return type;
}

// This method requires the caller have a valid NSAutoreleasePool
//
// This method works as follows:
//
// 1) Strip any pointer'dness from the arg type
// 2) If the resulting type != a base type, assume it needs a typedef
// 3) We *DO NOT* retain the original type. Everything that is typedef'd is forced to int, I.E.
//
//    original:   typedef float*** foo_t
//    encoded:    typedef int foo_t

static int dt_ld_probe_encode_typedef_iter(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
	NSMutableDictionary* types = data;
	dt_probe_t *probe = idp->di_data;
	dt_node_t* node;
	for (node = probe->pr_nargs; node != NULL; node = node->dn_list) {		
		ctf_id_t stripped_of_pointers = dt_ld_strip_pointers(node->dn_ctfp, node->dn_type);
		ctf_id_t base = ctf_type_resolve(node->dn_ctfp, stripped_of_pointers);
		if (base != stripped_of_pointers) {
			ssize_t size = ctf_type_lname(node->dn_ctfp, stripped_of_pointers, NULL, 0) + 1;
			char* buf = alloca(size);
			ctf_type_lname(node->dn_ctfp, stripped_of_pointers, buf, size);
			NSString* typeKey = [NSString stringWithUTF8String:buf];

			// Gah. DTrace always typedefs a certain set of types, which are not base types.
			// See <rdar://problem/5194316>. I haven't been able to discover a way to differentiate
			// the predefined types from those created in provider.d files, so we do this the hard
			// way.
			
			if ([encodingExclusionTypes objectForKey:typeKey] == nil) {
				if ([types objectForKey:typeKey] == nil) {
					[types setObject:dt_ld_encode_nsstring(typeKey) forKey:typeKey];
				}
			}
		}
	}
	
	return 0;
}

char* dt_ld_encode_typedefs(char* provider_name, dt_provider_t *provider)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	NSMutableDictionary* types = [NSMutableDictionary dictionary];
	
	if (encodingExclusionTypes == nil)
		encodingExclusionTypes = [dt_ld_create_encoding_exclusion_types() retain];

	dt_idhash_iter(provider->pv_probes, dt_ld_probe_encode_typedef_iter, types);
	
	NSArray* values = [types allValues];
	NSUInteger i, count = [values count];
	
	NSMutableString* string = [NSMutableString stringWithFormat:@"%s%s%s%s%s",
				   dtrace_typedefs_encoding_prefix,
				   dtrace_separator,
				   provider_name,
				   dtrace_separator,
				   dtrace_typedefs_version,
				   nil];
	
	for (i=0; i<count; i++) {
		[string appendFormat:@"%s%@", dtrace_separator, [values objectAtIndex:i]];
	}		
	
	char* value = strdup([string UTF8String]);
	
	if (_dtrace_debug) {
		NSEnumerator *enumerator = [types keyEnumerator];
		id key;
		while ((key = [enumerator nextObject])) {
			dt_dprintf("dt_ld encoding type %s as %s\n", [key UTF8String], [[types objectForKey:key] UTF8String]);
		}
	}
	
	[pool drain];
	
	return value;
}

NSString* dt_ld_decode_typedefs_v1(NSArray* typedefs)
{
	NSMutableString* decoded = [NSMutableString string];
	
	NSUInteger i, count = [typedefs count];
	for (i=3; i<count; i++) {
		[decoded appendFormat:@"typedef int %@;\n", dt_ld_decode_nsstring([typedefs objectAtIndex:i])];
	}
	
	return decoded;
}

NSString* dt_ld_decode_typedefs(NSString* encoding, NSString **version_out)
{
	NSArray* elements = [encoding componentsSeparatedByString:@dtrace_separator];
	
    NSString* version = [elements objectAtIndex:2];
    if (version_out) *version_out = version;
    
	// Is anything actually encoded?
	if ([elements count] > 3) {
		// Both v1 & v2 use the same format, v2 is a subset of v1 (with the fix for <rdar://problem/5194316>)
		if ([version isEqualToString:@"v1"] || [version isEqualToString:@"v2"])
			return dt_ld_decode_typedefs_v1(elements);
		else {
			// Wow, no good way to handle error conditions here.
			return [NSString stringWithFormat:@"Unhandled typedefs encoding version %@", version];
		}
	}
	
	return @"";
}

#pragma mark -
#pragma mark probe encoding / decoding

char* dt_ld_encode_probe(char* provider_name, char* probe_name, dt_probe_t* probe)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	NSMutableString* string = [NSMutableString stringWithFormat:@"%s%s%s%s%s%s%s",
				   dtrace_probe_encoding_prefix,
				   dtrace_separator,
				   provider_name, 
				   dtrace_separator,
				   probe_name,
				   dtrace_separator,
				   dtrace_probe_version,
				   nil];
	
	int i;
	for (i=0; i<probe->pr_nargc; i++) {
		dt_node_t* node = probe->pr_nargv[i];
		ssize_t size = ctf_type_lname(node->dn_ctfp, node->dn_type, NULL, 0) + 1;
		char* buf = alloca(size);
		ctf_type_lname(node->dn_ctfp, node->dn_type, buf, size);
		char* encoded_buf = dt_ld_encode_string(buf);
		
		[string appendFormat:@"%s%s", dtrace_separator, encoded_buf];
	}		
	
	char* value = strdup([string UTF8String]);
	
	[pool drain];
	
	return value;	
}

NSString* dt_ld_decode_probe_v1(NSArray* arguments)
{
	NSMutableString* decoded = [NSMutableString string];
	
	NSUInteger i, count = [arguments count];
	for (i=4; i<count; i++) {
		if (i+1 < count)
			[decoded appendFormat:@"%@,", dt_ld_decode_nsstring([arguments objectAtIndex:i])];
		else
			[decoded appendFormat:@"%@", dt_ld_decode_nsstring([arguments objectAtIndex:i])];
	}
	
	return decoded;	
}

NSString* dt_ld_decode_probe(NSString* encoding)
{
	NSArray* elements = [encoding componentsSeparatedByString:@dtrace_separator];
	
	NSMutableString* probe = [NSMutableString stringWithFormat:@"\tprobe %@(", [elements objectAtIndex:2]];
	
	if ([elements count] > 4) {
		NSString* version = [elements objectAtIndex:3];
		
		if ([version isEqualToString:@"v1"])
			[probe appendFormat:@"%@", dt_ld_decode_probe_v1(elements)];
		else {
			// Wow, no good way to handle error conditions here.
			[probe appendFormat:@"Unhandled probe encoding version %@", version];
		}
	}
	
	[probe appendFormat:@");"];
	
	return probe;
}


#pragma mark -
#pragma mark isenabled encoding

char* dt_ld_encode_isenabled(char* provider_name, char* probe_name)
{
	// "isenabled" probe info is encoded as (dtrace_isenabled_encoding_prefix)(providerName)(probe_name)
	size_t bufsize = sizeof(dtrace_isenabled_encoding_prefix) +
	sizeof(dtrace_separator) +
	strlen(provider_name) +
	sizeof(dtrace_separator) +
	strlen(probe_name) +
	1; // NULL terminator
	
	char* buffer = malloc(bufsize); 
	
	snprintf(buffer, bufsize, "%s%s%s%s%s%s%s",
		 dtrace_isenabled_encoding_prefix,
		 dtrace_separator,
		 provider_name,
		 dtrace_separator,
		 probe_name,
		 dtrace_separator,
		 dtrace_isenabled_version);
	
	return buffer;
}

#pragma mark -
#pragma mark D Script regeneration

NSString* dt_ld_decode_script(NSString* stability, NSString* typedefs, NSArray* probes)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	NSString* decodedTypedefs = dt_ld_decode_typedefs(typedefs, nil);
	
	NSMutableString* script = [[NSMutableString alloc] initWithFormat:@"%@\n", decodedTypedefs];
	
	// Maybe we should pass the provider name in? Do some error checking?
	NSString* provider = [[stability componentsSeparatedByString:@dtrace_separator] objectAtIndex:1];
	[script appendFormat:@"provider %@ {\n", provider];
	
	NSMutableDictionary* uniquedProbes = [NSMutableDictionary dictionary];
	for (NSString* probe in probes) {
		NSArray* components = [probe componentsSeparatedByString:@dtrace_separator];
		
		if ([components count] < 3) // Can't be a probe spec
			continue;
		
		if (![[components objectAtIndex:0] isEqualToString:@dtrace_probe_decoding_prefix])
			continue;
		
		NSString* probeName = [components objectAtIndex:2];
		if ([uniquedProbes objectForKey:probeName] != nil)
			continue;
		
		[uniquedProbes setObject:probeName forKey:probeName];
		[script appendFormat:@"%@\n", dt_ld_decode_probe(probe)];
	}
	[script appendFormat:@"};\n\n", provider];
	
	[script appendFormat:@"%@\n", dt_ld_decode_stability(stability)];
	
	[pool drain];
	
	return [script autorelease];
}

#pragma mark -
#pragma mark Linker support

static int linker_flags(cpu_type_t cpu)
{
	int oflags = 0;
	
	oflags |= DTRACE_O_NODEV;
	
	if(cpu & CPU_ARCH_ABI64)
		oflags |= DTRACE_O_LP64;
	else
		oflags |= DTRACE_O_ILP32;
	
	return oflags;
}

static void set_options(dtrace_hdl_t* dtp)
{
	(void) dtrace_setopt(dtp, "linkmode", "dynamic");
	(void) dtrace_setopt(dtp, "unodefs", NULL);
	(void) dtrace_setopt(dtp, "nolibs", NULL); /* In case /usr/lib/dtrace/* is broken, we can succeed. */
}

static int register_probes(dtrace_hdl_t* dtp, int count, const char* labels[], const char* functions[])
{
	int i;
	int is_enabled = 0;
	
	for(i = 0; i < count; i++) {
		const char* label = labels[i];
		const char* label0 = label;
		
		if(strncmp(label, dtrace_isenabled_decoding_prefix, sizeof(dtrace_isenabled_decoding_prefix) - 1) == 0) {			
			// skip prefix
			label += sizeof(dtrace_isenabled_decoding_prefix) - 1;
			is_enabled = 1;
		} else if (strncmp(label, dtrace_probe_decoding_prefix, sizeof(dtrace_probe_decoding_prefix) - 1) == 0) {			
			// skip prefix
			label += sizeof(dtrace_probe_decoding_prefix) - 1;
			is_enabled = 0;
		} else {
			fprintf(stderr, "error: invalid probe marker: %s\n", label0);
			return -1;
		}
		
		// skip separator
		
		label += sizeof(dtrace_separator) - 1;
		
		// Grab the provider name
		
		char* end = strstr(label, dtrace_separator);
		if(!end) {
			fprintf(stderr, "error: probe marker contains no provider name: %s\n", label0);
			return -1;
		}
		
		char* provider_name = malloc(end - label + 1);
		memcpy(provider_name, label, (end - label));
		provider_name[end - label] = 0;
		
		// Skip the separator
		
		label = end + sizeof(dtrace_separator) - 1;
		
		// Grab the probe name
		
		end = strstr(label, dtrace_separator);
		
		if(!end) {
			fprintf(stderr, "error: probe marker contains no probe name: %s\n", label0);
			return -1;
		}
		
		char* probe_name = malloc(end - label + 1);
		memcpy(probe_name, label, (end - label));
		probe_name[end - label] = 0;
		probe_name = strhyphenate(probe_name);
		
		// now, register the probe
		dt_provider_t *provider = dt_provider_lookup(dtp, provider_name);
		if(!provider) {
			fprintf(stderr, "error: provider %s doesn't exist\n", provider_name);
			return -1;
		}
		
		dt_probe_t* probe = dt_probe_lookup(provider, probe_name);
		if(!probe) {
			fprintf(stderr, "error: probe %s doesn't exist\n", probe_name);
			return -1;
		}
		
		// The "raw" function names provided by the linker will have an underscore
		// prepended. Remove it before registering the probe function.
		const char* function_name = functions[i];
		if (function_name[0] == '_')
			function_name++;
		
		if(dt_probe_define(provider, probe, function_name, NULL, i, is_enabled)) {
			fprintf(stderr, "error: couldn't define probe %s:::%s\n", provider_name, probe_name);
			return -1;
		}
		
		free(provider_name);            // free() of provider_name
		free(probe_name);               // free() of probe_name
	}
	
	return 0;
}

int register_offsets(dof_hdr_t* header, int count, uint64_t offsetsInDOF[])
{
	dof_sec_t* sections = (dof_sec_t*)((char*)header + header->dofh_secoff);
	
	int i;
	
	for(i = 0; i < count; i++)
		offsetsInDOF[i] = (uint64_t)-1;
	
	for(i = 0; i < header->dofh_secnum; i++) {
		switch(sections[i].dofs_type) {
		case DOF_SECT_PROFFS:
		case DOF_SECT_PRENOFFS:
			{
				// As each probe is defined, it gets a uint32_t entry that indicates its offset.
				// In the Sun DOF, this is the offset from the start of the function the probe
				// resides in. We use it to mean "offset to this probe from the start of the
				// DOF section". We stored the relocation_index as a placeholder in the
				// register_probes() function.
				uint32_t* probe_offsets = (uint32_t*)((char*)header + sections[i].dofs_offset);				
				uint32_t j, count = sections[i].dofs_size / sizeof(uint32_t);
				for (j=0; j<count; j++) {
					int relocation_index = probe_offsets[j];
					offsetsInDOF[relocation_index] = (uint64_t)(unsigned long)((char*)&probe_offsets[j] - (char*)header);
				}
			}
			break;
		}
	}
	
	return 0;
}

void* dtrace_ld_create_dof(cpu_type_t cpu,             // [provided by linker] target architecture
			   unsigned int typeCount,     // [provided by linker] number of stability or typedef symbol names
			   const char* typeNames[],    // [provided by linker] stability or typedef symbol names
			   unsigned int probeCount,    // [provided by linker] number of probe or isenabled locations
			   const char* probeNames[],   // [provided by linker] probe or isenabled symbol names
			   const char* probeWithin[],  // [provided by linker] function name containing probe or isenabled
			   uint64_t offsetsInDOF[],    // [allocated by linker, populated by DTrace] per-probe offset in the DOF
			   size_t* size)               // [allocated by linker, populated by DTrace] size of the DOF)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	BOOL printReconstructedScript = getenv("DTRACE_PRINT_RECONSTRUCTED_SCRIPT") != NULL;

	int i, err;
	const char* stability = NULL;
	const char* typedefs = NULL;
	
	// First, find a valid stability and typedefs.
	for (i=0; i<typeCount; i++) {
		if (strncmp(typeNames[i], dtrace_stability_decoding_prefix, sizeof(dtrace_stability_decoding_prefix)-1) == 0) {
			if (stability == NULL) {
				stability = typeNames[i];
			} else if (strcmp(stability, typeNames[i]) != 0) {
				fprintf(stderr, "error: Found conflicting dtrace stability info:\n%s\n%s\n", stability, typeNames[i]);
				return NULL;
			}
		} else if (strncmp(typeNames[i], dtrace_typedefs_decoding_prefix, sizeof(dtrace_typedefs_decoding_prefix)-1) == 0) {
			if (typedefs == NULL) {
				typedefs = typeNames[i];
			} else if (strcmp(typedefs, typeNames[i]) != 0) {
                // let's see if it's from a version conflict.
                NSString *existing_info = nil, *new_info = nil;
                dt_ld_decode_typedefs([NSString stringWithUTF8String:typedefs], &existing_info);
                dt_ld_decode_typedefs([NSString stringWithUTF8String:typeNames[i]], &new_info);
                if ([existing_info isEqualToString:new_info] == NO) {
                    fprintf(stderr, 
                            "error: Found dtrace typedefs generated by "
                            "different versions of dtrace:\n%s (%s)\n%s (%s)\n", 
                            typedefs, [existing_info UTF8String], 
                            typeNames[i], [new_info UTF8String]);
                    fprintf(stderr, "Please try regenerating all dtrace created "
                            "header files with the same version of "
                            "dtrace before rebuilding your project.\n");
                } else {
                    fprintf(stderr, "error: Found conflicting dtrace typedefs info:\n%s\n%s\n", typedefs, typeNames[i]);
                }
				return NULL;
			}			
		} else {
			fprintf(stderr, "error: Found unhandled dtrace typename prefix: %s\n", typeNames[i]);
			return NULL;
		}
	}
	
	if (stability == NULL) {
		fprintf(stderr, "error: Must have a valid dtrace stability entry\n");
		return NULL;
	}
	
	if (typedefs == NULL) {
		fprintf(stderr, "error: Must have a a valid dtrace typedefs entry\n");
		return NULL;
	}
	
	// Recreate the provider.d script
	NSMutableArray* probes = [NSMutableArray arrayWithCapacity:probeCount];
	int is_enabled_probes = 0;
	for (i=0; i<probeCount; i++) {
		if (strncmp(probeNames[i], dtrace_probe_decoding_prefix, sizeof(dtrace_probe_decoding_prefix)-1) == 0) {
			// Assert this belongs to the correct provider!
			[probes addObject:[NSString stringWithUTF8String:probeNames[i]]];
		} else if (strncmp(probeNames[i], dtrace_isenabled_decoding_prefix, sizeof(dtrace_isenabled_decoding_prefix)-1) == 0) {
			// Assert this belongs to the correct provider!
			[probes addObject:[NSString stringWithUTF8String:probeNames[i]]];
			is_enabled_probes++;
		} else {
			fprintf(stderr, "error: Found unhandled dtrace probe prefix: %s\n", probeNames[i]);
		}
	}
	
	if ([probes count] == 0) {
		fprintf(stderr, "error: Dtrace provider %s has no probes\n", stability);
		return NULL;
	}
	
	NSString* dscript = dt_ld_decode_script([NSString stringWithUTF8String:stability],
						[NSString stringWithUTF8String:typedefs],
						probes);	
	
	dtrace_hdl_t* dtp = dtrace_open(DTRACE_VERSION, 
	                                linker_flags(cpu), 
					&err);
	
	if(!dtp) {
		fprintf(stderr,"error: Failed to initialize dtrace: %s\n", dtrace_errmsg(NULL, err));
		return NULL;
	}
	
	set_options(dtp);
	
	dtrace_prog_t* program = dtrace_program_strcompile(dtp, 
							   [dscript UTF8String],
							   DTRACE_PROBESPEC_NONE,
							   0,
							   0,
							   NULL);
	
	if(!program) {
		fprintf(stderr, "error: Could not compile reconstructed dtrace script:\n\n%s\n", [dscript UTF8String]);
		return NULL;
	}
	
	if (_dtrace_debug || printReconstructedScript) {
		fprintf(stderr, "\n%s\n", [dscript UTF8String]);
	}
	
	if(register_probes(dtp, probeCount, probeNames, probeWithin)) {
		fprintf(stderr, "error: Could not register probes\n");
		return NULL;
	}
		
	dof_hdr_t* dof = dtrace_dof_create(dtp, program, DTRACE_D_PROBES | DTRACE_D_STRIP);
	
	if(register_offsets(dof, probeCount, offsetsInDOF)) {
		fprintf(stderr, "error: Could not register DOF offsets\n");
		return NULL;
	}
	
	*size = dof->dofh_filesz;
	
	void* return_data = malloc(*size);
	memcpy(return_data, dof, *size);
		
	if(needs_swapping(host_arch, cpu))
		dtrace_dof_byteswap((dof_hdr_t*)return_data);
	
	dtrace_dof_destroy(dtp, dof);
	dtrace_close(dtp);
	
	[pool drain];
	
	return return_data;
}
