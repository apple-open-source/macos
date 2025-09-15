/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#include "region.h"
#include "notes.h"

#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/types.h>

#ifndef _COREFILE_H
#define _COREFILE_H

#if defined(__LP64__)
typedef struct mach_header_64 native_mach_header_t;
typedef struct segment_command_64 native_segment_command_t;
#define NATIVE_MH_MAGIC		MH_MAGIC_64
#define NATIVE_LC_SEGMENT	LC_SEGMENT_64
#else
typedef struct mach_header native_mach_header_t;
typedef struct segment_command native_segment_command_t;
#define NATIVE_MH_MAGIC		MH_MAGIC
#define NATIVE_LC_SEGMENT	LC_SEGMENT
#endif

extern native_mach_header_t *make_corefile_mach_header(void *,
    const native_mach_header_t *);
extern native_mach_header_t *get_aout_mach_header(task_t);

extern struct note_command *make_task_crashinfo_note(native_mach_header_t *,
    struct note_command *, struct write_segment_data *,
    const struct task_crashinfo_note_data *);
extern struct note_command *make_region_infos_note(native_mach_header_t *,
    struct note_command *, struct write_segment_data *,
    const struct region_infos_note_data *);
extern struct note_command *make_addrable_bits_note(native_mach_header_t *,
    struct load_command *, struct write_segment_data *,
    const struct note_addrable_bits *);
extern struct note_command *make_all_image_infos_note(native_mach_header_t *,
    struct load_command *, struct write_segment_data *,
    const struct all_image_infos_note_data *);
extern struct note_command *make_process_metadata_note(native_mach_header_t *,
    struct load_command *, struct write_segment_data *,
    const struct process_metadata_note_data *);

extern mach_vm_size_t size_task_crashinfo_note(
    const struct task_crashinfo_note_data *);
extern mach_vm_size_t size_region_infos_note(
    const struct region_infos_note_data *);
extern mach_vm_size_t size_addrable_bits_note(
    const struct note_addrable_bits *);
extern mach_vm_size_t size_all_image_infos_note(
    const struct all_image_infos_note_data *);
extern mach_vm_size_t size_process_metadata_note(
    const struct process_metadata_note_data *);

struct note_command;
extern void validate_note_content(const struct note_command *, const char *, int);

extern void set_collect_phys_footprint(bool);

static __inline void mach_header_inc_ncmds(native_mach_header_t *mh, uint32_t inc) {
    mh->ncmds += inc;
}

static __inline void mach_header_inc_sizeofcmds(native_mach_header_t *mh, uint32_t inc) {
    mh->sizeofcmds += inc;
}

struct size_core {
    unsigned long count;        /* number-of-objects */
    size_t headersize;          /* size used in mach header */
    mach_vm_offset_t memsize;   /* size copied from memory */
    mach_vm_offset_t zfodsize;  /* size of zero-fill segments */
    task_t task;
};

struct size_segment_data {
    struct size_core ssd_vanilla;  /* full segments with data */
    struct size_core ssd_sparse;   /* sparse segments with data */
};

struct write_segment_data {
    task_t wsd_task;
    native_mach_header_t *wsd_mh;
    void *wsd_lc;
    int wsd_fd;
    bool wsd_nocache;
    off_t wsd_foffset;
    off_t wsd_nwritten;
};

#endif /* _COREFILE_H */
