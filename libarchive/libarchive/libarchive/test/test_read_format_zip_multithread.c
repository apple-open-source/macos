/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2019 Apple Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

/*
 * This test extracts and verifies zip files with multiple
 * threads. It first creates a bunch of zip files
 * sequentially and passes those to extractor threads.
 * After extraction, it verifies if the contents match.
 */

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#define NUM_THREADS 8

static void
create_zip(struct archive *a, int dirnum)
{
    struct archive_entry *ae;
    char path[PATH_MAX];
    int i;
    unsigned long sz;

    sprintf(path, "dir%d", dirnum);

    assert((ae = archive_entry_new()) != NULL);
    archive_entry_copy_pathname(ae, path);
    archive_entry_set_mode(ae, S_IFDIR | 0755);
    archive_entry_set_size(ae, 512);
    assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
    failure("size should be zero so that applications know not to write");
    assertEqualInt(0, archive_entry_size(ae));
    archive_entry_free(ae);
    assertEqualIntA(a, 0, archive_write_data(a, "12345678", 9));

    for (i = 1; i <= 4; i++) {
        sprintf(path, "dir%d/dir%dfile%d", dirnum, dirnum, i);
        sz = strlen(path);

        assert((ae = archive_entry_new()) != NULL);
        archive_entry_copy_pathname(ae, path);
        archive_entry_set_mode(ae, AE_IFREG | 0644);
        archive_entry_set_size(ae, sz);
        assertEqualInt(0, archive_write_header(a, ae));
        archive_entry_free(ae);
        assertEqualInt(sz, archive_write_data(a, path, sz));
    }
}

static int
copy_data(struct archive *ar, struct archive *aw)
{
    int r;
    const void *buff;
    size_t size;
    la_int64_t offset;

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF)
            return (ARCHIVE_OK);
        if (r < ARCHIVE_OK)
            return (r);

        r = (int)archive_write_data_block(aw, buff, size, offset);
        if (r < ARCHIVE_OK)
            return (r);
    }
}

static void
extract_zip(const char* file_path)
{
    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int flags;
    int r;

    flags = ARCHIVE_EXTRACT_OWNER;
    flags |= ARCHIVE_EXTRACT_NO_OVERWRITE;
    flags |= ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;
    flags |= ARCHIVE_EXTRACT_XATTR;
    flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
    flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;

    a = archive_read_new();
    archive_read_support_format_all(a);

    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

    fprintf(stderr, "Extracting %s\n", file_path);
    r = archive_read_open_filename(a, file_path, 10240);

    if (r != ARCHIVE_OK)
        return;

    for (;;) {
        r = archive_read_next_header(a, &entry);

        if (r == ARCHIVE_EOF)
            break;
        if (r < ARCHIVE_OK)
            fprintf(stderr, "%s\n", archive_error_string(a));
        if (r < ARCHIVE_WARN)
            goto bail;

        r = archive_write_header(ext, entry);
        if (r < ARCHIVE_OK)
            fprintf(stderr, "%s\n", archive_error_string(ext));
        else if (archive_entry_size(entry) > 0) {
            r = copy_data(a, ext);
            if (r < ARCHIVE_OK)
                fprintf(stderr, "%s\n", archive_error_string(ext));
            if (r < ARCHIVE_WARN)
                goto bail;
        }
        r = archive_write_finish_entry(ext);
        if (r < ARCHIVE_OK)
            fprintf(stderr, "%s\n", archive_error_string(ext));
        if (r < ARCHIVE_WARN)
            goto bail;
    }

bail:
    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    fprintf(stderr, "Done with %s\n", file_path);
}

void * extractor_thread(void *ctx)
{
    extract_zip(ctx);
    return NULL;
}

static void
verify_zip(int dirnum)
{
    char path[PATH_MAX];
    int i;
    unsigned long sz;

    sprintf(path, "dir%d", dirnum);
    // Verify if the directory exists
    assertIsDir(path, 0755);

    for (i = 1; i <= 4; i++) {
        sprintf(path, "dir%d/dir%dfile%d", dirnum, dirnum, i);
        sz = strlen(path);

        // Verify the file existence, size and contents
        assertIsReg(path, 0644);
        assertFileSize(path, sz);
        assertFileContents(path, (int)sz, path);
    }
}

DEFINE_TEST(test_read_format_zip_multithread)
{
    struct archive *a;
    int i;
    size_t used;
    size_t buffsize = 1000000;
    char *buff, archive_path[PATH_MAX];
    pthread_t thread_ids[NUM_THREADS];
    char path_list[NUM_THREADS][PATH_MAX];

    buff = malloc(buffsize);

    for (i = 0; i < NUM_THREADS; i++) {
        assert((a = archive_write_new()) != NULL);
        assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
        assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
        assertEqualIntA(a, ARCHIVE_OK,
                        archive_write_open_memory(a, buff, buffsize, &used));
        create_zip(a, i);
        assertEqualInt(ARCHIVE_OK, archive_write_close(a));
        assertEqualInt(ARCHIVE_OK, archive_write_free(a));

        sprintf(archive_path, "archive%d.zip", i);
        dumpfile(archive_path, buff, used);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        sprintf(path_list[i], "archive%d.zip", i);
        pthread_create(&thread_ids[i], NULL, extractor_thread, path_list[i]);
    }

    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(thread_ids[i], NULL);

    for (i = 0; i < NUM_THREADS; i++)
        verify_zip(i);

    free(buff);
}
