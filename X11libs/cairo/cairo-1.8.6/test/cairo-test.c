/*
 * Copyright © 2004 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#define _GNU_SOURCE 1	/* for feenableexcept() et al */
#define _POSIX_C_SOURCE 2000112L /* for flockfile() et al */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#if HAVE_FEENABLEEXCEPT
#include <fenv.h>
#endif
#include <assert.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#if HAVE_FCFINI
#include <fontconfig/fontconfig.h>
#endif
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_VALGRIND
#include <valgrind.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

#if HAVE_MEMFAULT
#include <memfault.h>
#define MF(x) x
#else
#define MF(x)
#endif

#include "cairo-test.h"

#include "buffer-diff.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#define vsnprintf _vsnprintf
#define access _access
#define F_OK 0
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

static cairo_user_data_key_t _cairo_test_context_key;

static void
_xunlink (const cairo_test_context_t *ctx, const char *pathname);

static const char *fail_face = "", *normal_face = "";

#define CAIRO_TEST_LOG_SUFFIX ".log"
#define CAIRO_TEST_PNG_SUFFIX "-out.png"
#define CAIRO_TEST_REF_SUFFIX "-ref.png"
#define CAIRO_TEST_DIFF_SUFFIX "-diff.png"

#define NUM_DEVICE_OFFSETS 1

static const char *vector_ignored_tests[] = {
    /* We can't match the results of tests that depend on
     * CAIRO_ANTIALIAS_NONE/SUBPIXEL for vector backends
     * (nor do we care). */
    /* XXX Perhaps this should be moved to a flag in cairo_test_t? */
    "a1-image-sample",
    "a1-traps-sample",
    "ft-text-antialias-none",
    "rectangle-rounding-error",
    "text-antialias-gray",
    "text-antialias-none",
    "text-antialias-subpixel",
    "text-lcd-filter-fir3",
    "text-lcd-filter-fir5",
    "text-lcd-filter-intra-pixel",
    "text-lcd-filter-none",
    "unantialiased-shapes",

    /* Nor do we care about rendering anomalies in external renderers. */
    "fill-degenerate-sort-order",
    NULL
};

static void
_cairo_test_init (cairo_test_context_t *ctx,
		  const cairo_test_t *test,
		  const char *test_name,
		  cairo_test_status_t expectation)
{
    char *log_name;

    MF (VALGRIND_DISABLE_FAULTS ());

#if HAVE_FEENABLEEXCEPT
    feenableexcept (FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
#endif

    ctx->test = test;
    ctx->test_name = test_name;
    ctx->expectation = expectation;

    ctx->malloc_failure = 0;
#if HAVE_MEMFAULT
    if (getenv ("CAIRO_TEST_MALLOC_FAILURE"))
	ctx->malloc_failure = atoi (getenv ("CAIRO_TEST_MALLOC_FAILURE"));
    if (ctx->malloc_failure && ! RUNNING_ON_MEMFAULT ())
	ctx->malloc_failure = 0;
#endif

    xasprintf (&log_name, "%s%s", test_name, CAIRO_TEST_LOG_SUFFIX);
    _xunlink (NULL, log_name);

    ctx->log_file = fopen (log_name, "a");
    if (ctx->log_file == NULL) {
	fprintf (stderr, "Error opening log file: %s\n", log_name);
	ctx->log_file = stderr;
    }
    free (log_name);

    ctx->srcdir = getenv ("srcdir");
    if (ctx->srcdir == NULL)
	ctx->srcdir = ".";

    ctx->refdir = getenv ("CAIRO_REF_DIR");

    ctx->ref_name = NULL;
    ctx->ref_image = NULL;
    ctx->ref_image_flattened = NULL;

    ctx->thread = 0;

    {
	int tmp_num_targets;
	cairo_bool_t tmp_limited_targets;
	ctx->targets_to_test = cairo_boilerplate_get_targets (&tmp_num_targets, &tmp_limited_targets);
	ctx->num_targets = tmp_num_targets;
	ctx->limited_targets = tmp_limited_targets;
    }

    printf ("\nTESTING %s\n", test_name);
}

void
cairo_test_init (cairo_test_context_t *ctx,
		 const char *test_name)
{
    _cairo_test_init (ctx, NULL, test_name, CAIRO_TEST_SUCCESS);
}

static void
cairo_test_init_thread (cairo_test_context_t *ctx,
			cairo_test_context_t *master,
			int thread)
{
    MF (VALGRIND_DISABLE_FAULTS ());

    *ctx = *master;
    ctx->thread = thread;
}

void
cairo_test_fini (cairo_test_context_t *ctx)
{
    if (ctx->thread != 0)
	return;

    if (ctx->log_file == NULL)
	return;

    if (ctx->log_file != stderr)
	fclose (ctx->log_file);
    ctx->log_file = NULL;

    if (ctx->ref_name != NULL)
	free (ctx->ref_name);
    cairo_surface_destroy (ctx->ref_image);
    cairo_surface_destroy (ctx->ref_image_flattened);

    cairo_boilerplate_free_targets (ctx->targets_to_test);

    cairo_debug_reset_static_data ();
#if HAVE_FCFINI
    FcFini ();
#endif
}

void
cairo_test_log (const cairo_test_context_t *ctx, const char *fmt, ...)
{
    va_list va;
    FILE *file = ctx && ctx->log_file ? ctx->log_file : stderr;

    va_start (va, fmt);
    vfprintf (file, fmt, va);
    va_end (va);
}

void
cairo_test_log_path (const cairo_test_context_t *ctx,
		     const cairo_path_t *path)
{
  int i;

  for (i = 0; i < path->num_data; i += path->data[i].header.length) {
    cairo_path_data_t *data = &path->data[i];
    switch (data->header.type) {
    case CAIRO_PATH_MOVE_TO:
	cairo_test_log (ctx,
		        "    cairo_move_to (cr, %g, %g);\n",
			data[1].point.x, data[1].point.y);
        break;
    case CAIRO_PATH_LINE_TO:
	cairo_test_log (ctx,
		        "    cairo_line_to (cr, %g, %g);\n",
			data[1].point.x, data[1].point.y);
	break;
    case CAIRO_PATH_CURVE_TO:
	cairo_test_log (ctx,
		        "    cairo_curve_to (cr, %g, %g, %g, %g, %g, %g);\n",
			data[1].point.x, data[1].point.y,
			data[2].point.x, data[2].point.y,
			data[3].point.x, data[3].point.y);
	break;
    case CAIRO_PATH_CLOSE_PATH:
	cairo_test_log (ctx,
		        "    cairo_close_path (cr);\n\n");
	break;
    default:
	assert (0);
    }
  }
}

static void
_xunlink (const cairo_test_context_t *ctx, const char *pathname)
{
    if (unlink (pathname) < 0 && errno != ENOENT) {
	cairo_test_log (ctx, "Error: Cannot remove %s: %s\n",
			pathname, strerror (errno));
	exit (1);
    }
}

char *
cairo_test_reference_image_filename (const cairo_test_context_t *ctx,
	                             const char *base_name,
				     const char *test_name,
				     const char *target_name,
				     const char *format)
{
    char *ref_name = NULL;

    /* First look for a previous build for comparison. */
    if (ctx->refdir != NULL) {
	xasprintf (&ref_name, "%s/%s%s",
		   ctx->refdir,
		   base_name,
		   CAIRO_TEST_PNG_SUFFIX);
	if (access (ref_name, F_OK) != 0)
	    free (ref_name);
	else
	    goto done;
    }

    /* Next look for a target/format-specific reference image. */
    xasprintf (&ref_name, "%s/%s-%s-%s%s", ctx->srcdir,
	       test_name,
	       target_name,
	       format,
	       CAIRO_TEST_REF_SUFFIX);
    if (access (ref_name, F_OK) != 0)
	free (ref_name);
    else
	goto done;

    /* Next, look for target-specific reference image. */
    xasprintf (&ref_name, "%s/%s-%s%s", ctx->srcdir,
	       test_name,
	       target_name,
	       CAIRO_TEST_REF_SUFFIX);
    if (access (ref_name, F_OK) != 0)
	free (ref_name);
    else
	goto done;

    /* Next, look for format-specific reference image. */
    xasprintf (&ref_name, "%s/%s-%s%s", ctx->srcdir,
	       test_name,
	       format,
	       CAIRO_TEST_REF_SUFFIX);
    if (access (ref_name, F_OK) != 0)
	free (ref_name);
    else
	goto done;

    /* Finally, look for the standard reference image. */
    xasprintf (&ref_name, "%s/%s%s", ctx->srcdir,
	       test_name,
	       CAIRO_TEST_REF_SUFFIX);
    if (access (ref_name, F_OK) != 0)
	free (ref_name);
    else
	goto done;

    ref_name = NULL;

done:
    return ref_name;
}

static cairo_bool_t
cairo_test_target_has_similar (const cairo_test_context_t *ctx,
			       const cairo_boilerplate_target_t *target)
{
    cairo_surface_t *surface;
    cairo_bool_t has_similar;
    cairo_t * cr;
    cairo_surface_t *similar;
    cairo_status_t status;
    void *closure;

    /* ignore image intermediate targets */
    if (target->expected_type == CAIRO_SURFACE_TYPE_IMAGE)
	return FALSE;

    if (getenv ("CAIRO_TEST_IGNORE_SIMILAR"))
	return FALSE;

    do {
	do {
	    surface = (target->create_surface) (ctx->test->name,
						target->content,
						ctx->test->width,
						ctx->test->height,
						ctx->test->width + 25 * NUM_DEVICE_OFFSETS,
						ctx->test->height + 25 * NUM_DEVICE_OFFSETS,
						CAIRO_BOILERPLATE_MODE_TEST,
						0,
						&closure);
	    if (surface == NULL)
		return FALSE;
	} while (cairo_test_malloc_failure (ctx, cairo_surface_status (surface)));

	if (cairo_surface_status (surface))
	    return FALSE;


	cr = cairo_create (surface);
	cairo_push_group_with_content (cr,
				       cairo_boilerplate_content (target->content));
	similar = cairo_get_group_target (cr);
	status = cairo_surface_status (similar);

	has_similar = cairo_surface_get_type (similar) == cairo_surface_get_type (surface);

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	if (target->cleanup)
	    target->cleanup (closure);
    } while (cairo_test_malloc_failure (ctx, status));

    return has_similar;
}

static cairo_surface_t *
_cairo_test_flatten_reference_image (cairo_test_context_t *ctx,
				     cairo_bool_t flatten)
{
    cairo_surface_t *surface;
    cairo_t *cr;

    if (! flatten)
	return ctx->ref_image;

    if (ctx->ref_image_flattened != NULL)
	return ctx->ref_image_flattened;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					  cairo_image_surface_get_width (ctx->ref_image),
					  cairo_image_surface_get_height (ctx->ref_image));
    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);

    cairo_set_source_surface (cr, ctx->ref_image, 0, 0);
    cairo_paint (cr);

    surface = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);

    if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
	ctx->ref_image_flattened = surface;
    return surface;
}

cairo_surface_t *
cairo_test_get_reference_image (cairo_test_context_t *ctx,
				const char *filename,
				cairo_bool_t flatten)
{
    cairo_surface_t *surface;
    int len;

    if (ctx->ref_name != NULL) {
	if (strcmp (ctx->ref_name, filename) == 0)
	    return _cairo_test_flatten_reference_image (ctx, flatten);

	cairo_surface_destroy (ctx->ref_image);
	ctx->ref_image = NULL;

	cairo_surface_destroy (ctx->ref_image_flattened);
	ctx->ref_image_flattened = NULL;

	free (ctx->ref_name);
	ctx->ref_name = NULL;
    }

    surface = cairo_image_surface_create_from_png (filename);
    if (cairo_surface_status (surface))
	return surface;

    len = strlen (filename);
    ctx->ref_name = xmalloc (len + 1);
    memcpy (ctx->ref_name, filename, len + 1);

    ctx->ref_image = surface;
    return _cairo_test_flatten_reference_image (ctx, flatten);
}

static cairo_bool_t
cairo_test_file_is_older (const char *filename,
	                  const char *ref_filename)
{
#if HAVE_SYS_STAT_H
    struct stat st, ref;

    if (stat (filename, &st) < 0)
	return FALSE;

    if (stat (ref_filename, &ref) < 0)
	return TRUE;

    return st.st_mtime < ref.st_mtime;
#else
    /* XXX */
    return FALSE;
#endif
}

static cairo_bool_t
cairo_test_files_equal (const char *test_filename,
			const char *pass_filename)
{
    FILE *test, *pass;
    int t, p;

    test = fopen (test_filename, "rb");
    if (test == NULL)
	return FALSE;

    pass = fopen (pass_filename, "rb");
    if (pass == NULL) {
	fclose (test);
	return FALSE;
    }

    /* as simple as it gets */
    do {
	t = getc (test);
	p = getc (pass);
	if (t != p)
	    break;
    } while (t != EOF && p != EOF);

    fclose (pass);
    fclose (test);

    return t == p; /* both EOF */
}

static cairo_bool_t
cairo_test_copy_file (const char *src_filename,
		      const char *dst_filename)
{
    FILE *src, *dst;
    int c;

#if HAVE_LINK
    if (link (src_filename, dst_filename) == 0)
	return TRUE;

    unlink (dst_filename);
#endif

    src = fopen (src_filename, "rb");
    if (src == NULL)
	return FALSE;

    dst = fopen (dst_filename, "wb");
    if (dst == NULL) {
	fclose (src);
	return FALSE;
    }

    /* as simple as it gets */
    while ((c = getc (src)) != EOF)
	putc (c, dst);

    fclose (src);
    fclose (dst);

    return TRUE;
}

static cairo_test_status_t
cairo_test_for_target (cairo_test_context_t		 *ctx,
		       const cairo_boilerplate_target_t	 *target,
		       int				  dev_offset,
		       cairo_bool_t                       similar)
{
    cairo_test_status_t status;
    cairo_surface_t *surface = NULL;
    cairo_t *cr;
    const char *empty_str = "";
    char *offset_str, *thread_str;
    char *base_name, *png_name, *ref_name, *diff_name;
    char *test_filename = NULL, *pass_filename = NULL, *fail_filename = NULL;
    cairo_test_status_t ret;
    cairo_content_t expected_content;
    cairo_font_options_t *font_options;
    const char *format;
    cairo_bool_t have_output = FALSE;
    cairo_bool_t have_result = FALSE;
    int malloc_failure_iterations = ctx->malloc_failure;
    void *closure;
    int width, height;
    int last_fault_count = 0;

    /* Get the strings ready that we'll need. */
    format = cairo_boilerplate_content_name (target->content);
    if (dev_offset)
	xasprintf (&offset_str, "-%d", dev_offset);
    else
	offset_str = (char *) empty_str;
    if (ctx->thread)
	xasprintf (&thread_str, "-thread%d", ctx->thread);
    else
	thread_str = (char *) empty_str;

    xasprintf (&base_name, "%s-%s-%s%s%s%s",
	       ctx->test->name,
	       target->name,
	       format,
	       similar ? "-similar" : "",
	       offset_str,
	       thread_str);

    if (offset_str != empty_str)
      free (offset_str);
    if (thread_str != empty_str)
      free (thread_str);


    ref_name = cairo_test_reference_image_filename (ctx,
						    base_name,
						    ctx->test->name,
						    target->name,
						    format);
    xasprintf (&png_name,  "%s%s", base_name, CAIRO_TEST_PNG_SUFFIX);
    xasprintf (&diff_name, "%s%s", base_name, CAIRO_TEST_DIFF_SUFFIX);

    if (target->is_vector) {
	int i;

	for (i = 0; vector_ignored_tests[i] != NULL; i++)
	    if (strcmp (ctx->test->name, vector_ignored_tests[i]) == 0) {
		cairo_test_log (ctx, "Error: Skipping for vector target %s\n", target->name);
		ret = CAIRO_TEST_UNTESTED;
		goto UNWIND_STRINGS;
	    }
    }

    width = ctx->test->width;
    height = ctx->test->height;
    if (width && height) {
	width += dev_offset;
	height += dev_offset;
    }

REPEAT:
#if HAVE_MEMFAULT
    VALGRIND_CLEAR_FAULTS ();
    VALGRIND_RESET_LEAKS ();
    ctx->last_fault_count = 0;
    last_fault_count = VALGRIND_COUNT_FAULTS ();
    VALGRIND_ENABLE_FAULTS ();
#endif
    have_output = FALSE;
    have_result = FALSE;

    /* Run the actual drawing code. */
    ret = CAIRO_TEST_SUCCESS;
    surface = (target->create_surface) (base_name,
					target->content,
					width, height,
					ctx->test->width + 25 * NUM_DEVICE_OFFSETS,
					ctx->test->height + 25 * NUM_DEVICE_OFFSETS,
					CAIRO_BOILERPLATE_MODE_TEST,
					ctx->thread,
					&closure);
    if (surface == NULL) {
	cairo_test_log (ctx, "Error: Failed to set %s target\n", target->name);
	ret = CAIRO_TEST_UNTESTED;
	goto UNWIND_STRINGS;
    }

    if (cairo_test_malloc_failure (ctx, cairo_surface_status (surface)))
	goto REPEAT;

    if (cairo_surface_status (surface)) {
	MF (VALGRIND_PRINT_FAULTS ());
	cairo_test_log (ctx, "Error: Created an error surface\n");
	ret = CAIRO_TEST_FAILURE;
	goto UNWIND_STRINGS;
    }

    /* Check that we created a surface of the expected type. */
    if (cairo_surface_get_type (surface) != target->expected_type) {
	MF (VALGRIND_PRINT_FAULTS ());
	cairo_test_log (ctx, "Error: Created surface is of type %d (expected %d)\n",
			cairo_surface_get_type (surface), target->expected_type);
	ret = CAIRO_TEST_FAILURE;
	goto UNWIND_SURFACE;
    }

    /* Check that we created a surface of the expected content,
     * (ignore the artificial CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED value).
     */
    expected_content = cairo_boilerplate_content (target->content);

    if (cairo_surface_get_content (surface) != expected_content) {
	MF (VALGRIND_PRINT_FAULTS ());
	cairo_test_log (ctx, "Error: Created surface has content %d (expected %d)\n",
			cairo_surface_get_content (surface), expected_content);
	ret = CAIRO_TEST_FAILURE;
	goto UNWIND_SURFACE;
    }

    cairo_surface_set_device_offset (surface, dev_offset, dev_offset);

    cr = cairo_create (surface);
    if (cairo_set_user_data (cr, &_cairo_test_context_key, (void*) ctx, NULL)) {
#if HAVE_MEMFAULT
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	if (target->cleanup)
	    target->cleanup (closure);

	goto REPEAT;
#else
	ret = CAIRO_TEST_FAILURE;
	goto UNWIND_CAIRO;
#endif
    }

    if (similar)
	cairo_push_group_with_content (cr, expected_content);

    /* Clear to transparent (or black) depending on whether the target
     * surface supports alpha. */
    cairo_save (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_restore (cr);

    /* Set all components of font_options to avoid backend differences
     * and reduce number of needed reference images. */
    font_options = cairo_font_options_create ();
    cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (font_options, CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_GRAY);
    cairo_set_font_options (cr, font_options);
    cairo_font_options_destroy (font_options);

    cairo_save (cr);
    status = (ctx->test->draw) (cr, ctx->test->width, ctx->test->height);
    cairo_restore (cr);

    if (similar) {
	cairo_pop_group_to_source (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint (cr);
    }

#if HAVE_MEMFAULT
    VALGRIND_DISABLE_FAULTS ();

    /* repeat test after malloc failure injection */
    if (ctx->malloc_failure &&
	VALGRIND_COUNT_FAULTS () - last_fault_count > 0 &&
	(status == CAIRO_TEST_NO_MEMORY ||
	 cairo_status (cr) == CAIRO_STATUS_NO_MEMORY ||
	 cairo_surface_status (surface) == CAIRO_STATUS_NO_MEMORY))
    {
	cairo_destroy (cr);
	cairo_surface_destroy (surface);
	if (target->cleanup)
	    target->cleanup (closure);
	if (ctx->thread == 0) {
	    cairo_debug_reset_static_data ();
#if HAVE_FCFINI
	    FcFini ();
#endif
	    if (VALGRIND_COUNT_LEAKS () > 0) {
		VALGRIND_PRINT_FAULTS ();
		VALGRIND_PRINT_LEAKS ();
	    }
	}

	goto REPEAT;
    }
#endif

    /* Then, check all the different ways it could fail. */
    if (status) {
	cairo_test_log (ctx, "Error: Function under test failed\n");
	ret = status;
	goto UNWIND_CAIRO;
    }

    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
	cairo_test_log (ctx, "Error: Function under test left cairo status in an error state: %s\n",
			cairo_status_to_string (cairo_status (cr)));
	ret = CAIRO_TEST_FAILURE;
	goto UNWIND_CAIRO;
    }

#if HAVE_MEMFAULT
    if (VALGRIND_COUNT_FAULTS () - last_fault_count > 0) {
	VALGRIND_PRINTF ("Unreported memfaults...");
	VALGRIND_PRINT_FAULTS ();
    }
#endif

    /* Skip image check for tests with no image (width,height == 0,0) */
    if (ctx->test->width != 0 && ctx->test->height != 0) {
	cairo_surface_t *ref_image;
	cairo_surface_t *test_image;
	cairo_surface_t *diff_image;
	buffer_diff_result_t result;
	cairo_status_t diff_status;

	if (target->finish_surface != NULL) {
	    diff_status = target->finish_surface (surface);
	    if (diff_status) {
		cairo_test_log (ctx, "Error: Failed to finish surface: %s\n",
				cairo_status_to_string (diff_status));
		ret = CAIRO_TEST_FAILURE;
		goto UNWIND_CAIRO;
	    }
	}

	if (ref_name == NULL) {
	    cairo_test_log (ctx, "Error: Cannot find reference image for %s\n",
			    base_name);

	    /* we may be running this test to generate reference images */
	    _xunlink (ctx, png_name);
	    test_image = target->get_image_surface (surface, 0,
		                                    ctx->test->width,
						    ctx->test->height);
	    diff_status = cairo_surface_write_to_png (test_image, png_name);
	    if (diff_status) {
		cairo_test_log (ctx,
			        "Error: Failed to write output image: %s\n",
			        cairo_status_to_string (diff_status));
	    }
	    have_output = TRUE;
	    cairo_surface_destroy (test_image);

	    ret = CAIRO_TEST_FAILURE;
	    goto UNWIND_CAIRO;
	}

	if (target->file_extension != NULL) { /* compare vector surfaces */
	    xasprintf (&test_filename, "%s-out%s",
		       base_name, target->file_extension);
	    xasprintf (&pass_filename, "%s-pass%s",
		       base_name, target->file_extension);
	    xasprintf (&fail_filename, "%s-fail%s",
		       base_name, target->file_extension);

	    if (cairo_test_file_is_older (pass_filename, ref_name))
		_xunlink (ctx, pass_filename);
	    if (cairo_test_file_is_older (fail_filename, ref_name))
		_xunlink (ctx, fail_filename);

	    if (cairo_test_files_equal (test_filename, pass_filename)) {
		/* identical output as last known PASS */
		cairo_test_log (ctx, "Vector surface matches last pass.\n");
		have_output = TRUE;
		ret = CAIRO_TEST_SUCCESS;
		goto UNWIND_CAIRO;
	    }
	    if (cairo_test_files_equal (test_filename, fail_filename)) {
		/* identical output as last known FAIL, fail */
		cairo_test_log (ctx, "Vector surface matches last fail.\n");
		have_result = TRUE; /* presume these were kept around as well */
		have_output = TRUE;
		ret = CAIRO_TEST_FAILURE;
		goto UNWIND_CAIRO;
	    }
	}

	test_image = target->get_image_surface (surface, 0,
					        ctx->test->width,
						ctx->test->height);
	if (cairo_surface_status (test_image)) {
	    cairo_test_log (ctx, "Error: Failed to extract image: %s\n",
			    cairo_status_to_string (cairo_surface_status (test_image)));
	    cairo_surface_destroy (test_image);
	    ret = CAIRO_TEST_FAILURE;
	    goto UNWIND_CAIRO;
	}

	_xunlink (ctx, png_name);
	diff_status = cairo_surface_write_to_png (test_image, png_name);
	if (diff_status) {
	    cairo_test_log (ctx, "Error: Failed to write output image: %s\n",
			    cairo_status_to_string (diff_status));
	    cairo_surface_destroy (test_image);
	    ret = CAIRO_TEST_FAILURE;
	    goto UNWIND_CAIRO;
	}
	have_output = TRUE;

	/* binary compare png files (no decompression) */
	if (target->file_extension == NULL) {
	    xasprintf (&test_filename, "%s", png_name);
	    xasprintf (&pass_filename, "%s-pass.png", base_name);
	    xasprintf (&fail_filename, "%s-fail.png", base_name);

	    if (cairo_test_file_is_older (pass_filename, ref_name))
		_xunlink (ctx, pass_filename);
	    if (cairo_test_file_is_older (fail_filename, ref_name))
		_xunlink (ctx, fail_filename);

	    if (cairo_test_files_equal (test_filename, pass_filename)) {
		/* identical output as last known PASS, pass */
		cairo_test_log (ctx, "PNG file exactly matches last pass.\n");
		cairo_surface_destroy (test_image);
		ret = CAIRO_TEST_SUCCESS;
		goto UNWIND_CAIRO;
	    }
	    if (cairo_test_files_equal (png_name, ref_name)) {
		/* identical output as reference image */
		cairo_test_log (ctx, "PNG file exactly reference image.\n");
		cairo_surface_destroy (test_image);
		ret = CAIRO_TEST_SUCCESS;
		goto UNWIND_CAIRO;
	    }

	    if (cairo_test_files_equal (test_filename, fail_filename)) {
		cairo_test_log (ctx, "PNG file exactly matches last fail.\n");
		/* identical output as last known FAIL, fail */
		have_result = TRUE; /* presume these were kept around as well */
		cairo_surface_destroy (test_image);
		ret = CAIRO_TEST_FAILURE;
		goto UNWIND_CAIRO;
	    }
	} else {
	    if (cairo_test_files_equal (png_name, ref_name)) {
		cairo_test_log (ctx, "PNG file exactly matches reference image.\n");
		cairo_surface_destroy (test_image);
		ret = CAIRO_TEST_SUCCESS;
		goto UNWIND_CAIRO;
	    }
	}

	ref_image = cairo_test_get_reference_image (ctx, ref_name,
						    target->content == CAIRO_TEST_CONTENT_COLOR_ALPHA_FLATTENED);
	if (cairo_surface_status (ref_image)) {
	    cairo_test_log (ctx, "Error: Cannot open reference image for %s: %s\n",
			    ref_name,
			    cairo_status_to_string (cairo_surface_status (ref_image)));
	    cairo_surface_destroy (ref_image);
	    cairo_surface_destroy (test_image);
	    ret = CAIRO_TEST_FAILURE;
	    goto UNWIND_CAIRO;
	}

	diff_image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						 ctx->test->width,
						 ctx->test->height);

	diff_status = image_diff (ctx,
				  test_image, ref_image,
				  diff_image,
				  &result);
	_xunlink (ctx, diff_name);
	if (diff_status) {
	    cairo_test_log (ctx, "Error: Failed to compare images: %s\n",
			    cairo_status_to_string (diff_status));
	    ret = CAIRO_TEST_FAILURE;
	}
	else if (result.pixels_changed &&
		 result.max_diff > target->error_tolerance)
	{
	    ret = CAIRO_TEST_FAILURE;

	    diff_status = cairo_surface_write_to_png (diff_image, diff_name);
	    if (diff_status) {
		cairo_test_log (ctx, "Error: Failed to write differences image: %s\n",
				cairo_status_to_string (diff_status));
	    } else
		have_result = TRUE;

	    cairo_test_copy_file (test_filename, fail_filename);
	} else { /* success */
	    cairo_test_copy_file (test_filename, pass_filename);
	}

	cairo_surface_destroy (test_image);
	cairo_surface_destroy (diff_image);
    }

UNWIND_CAIRO:
    if (test_filename != NULL) {
	free (test_filename);
	test_filename = NULL;
    }
    if (fail_filename != NULL) {
	free (fail_filename);
	fail_filename = NULL;
    }
    if (pass_filename != NULL) {
	free (pass_filename);
	pass_filename = NULL;
    }

#if HAVE_MEMFAULT
    if (ret == CAIRO_TEST_FAILURE && ctx->expectation != CAIRO_TEST_FAILURE)
	VALGRIND_PRINT_FAULTS ();
#endif
    cairo_destroy (cr);
UNWIND_SURFACE:
    cairo_surface_destroy (surface);

    if (target->cleanup)
	target->cleanup (closure);

#if HAVE_MEMFAULT
    if (ctx->thread == 0) {
	cairo_debug_reset_static_data ();

#if HAVE_FCFINI
	FcFini ();
#endif

	if (VALGRIND_COUNT_LEAKS () > 0) {
	    if (ret != CAIRO_TEST_FAILURE ||
		ctx->expectation == CAIRO_TEST_FAILURE)
	    {
		VALGRIND_PRINT_FAULTS ();
	    }
	    VALGRIND_PRINT_LEAKS ();
	}
    }

    if (ret == CAIRO_TEST_SUCCESS && --malloc_failure_iterations > 0)
	goto REPEAT;
#endif

    if (ctx->thread == 0) {
	if (have_output)
	    cairo_test_log (ctx, "OUTPUT: %s\n", png_name);

	if (have_result) {
	    cairo_test_log (ctx,
		            "REFERENCE: %s\nDIFFERENCE: %s\n",
			    ref_name, diff_name);
	}
    }

UNWIND_STRINGS:
    if (png_name)
      free (png_name);
    if (ref_name)
      free (ref_name);
    if (diff_name)
      free (diff_name);
    if (base_name)
      free (base_name);

    return ret;
}

#if defined(HAVE_SIGNAL_H) && defined(HAVE_SETJMP_H)
#include <signal.h>
#include <setjmp.h>
/* Used to catch crashes in a test, so that we report it as such and
 * continue testing, although one crasher may already have corrupted memory in
 * an nonrecoverable fashion. */
static jmp_buf jmpbuf;

static void
segfault_handler (int signal)
{
    longjmp (jmpbuf, signal);
}
#endif

static cairo_test_status_t
cairo_test_run (cairo_test_context_t *ctx)
{
    /* we use volatile here to make sure values are not clobbered
     * by longjmp */
    volatile size_t i, j;
    volatile cairo_bool_t print_fail_on_stdout = ctx->thread == 0;
    volatile cairo_test_status_t status, ret;

#if HAVE_UNISTD_H
    if (ctx->thread == 0 && isatty (2)) {
	fail_face = "\033[41m\033[37m\033[1m";
	normal_face = "\033[m";
	if (isatty (1))
	    print_fail_on_stdout = FALSE;
    }
#endif

    /* The intended logic here is that we return overall SUCCESS
     * iff. there is at least one tested backend and that all tested
     * backends return SUCCESS, OR, there's backends were manually
     * limited, and none were tested.
     * In other words:
     *
     *  if      backends limited and no backend tested
     *          -> SUCCESS
     *	else if any backend not SUCCESS
     *		-> FAILURE
     *	else if all backends UNTESTED
     *		-> FAILURE
     *	else    (== some backend SUCCESS)
     *		-> SUCCESS
     *
     * Also, on a crash, run no further tests.
     */
    status = ret = CAIRO_TEST_UNTESTED;
    for (i = 0; i < ctx->num_targets && status != CAIRO_TEST_CRASHED; i++) {
	const cairo_boilerplate_target_t * volatile target = ctx->targets_to_test[(i + ctx->thread) % ctx->num_targets];

	for (j = 0; j < NUM_DEVICE_OFFSETS; j++) {
	    volatile int dev_offset = ((j + ctx->thread) % NUM_DEVICE_OFFSETS) * 25;
	    volatile int similar, has_similar;

	    has_similar = cairo_test_target_has_similar (ctx, target);
	    for (similar = 0; similar <= has_similar ; similar++) {
		cairo_test_log (ctx, "Testing %s with %s%s target (dev offset %d)\n", ctx->test_name, similar ? " (similar) " : "", target->name, dev_offset);
		if (ctx->thread == 0) {
		    printf ("%s-%s-%s [%d]%s:\t", ctx->test->name, target->name,
			    cairo_boilerplate_content_name (target->content),
			    dev_offset,
			    similar ? " (similar) ": "");
		    fflush (stdout);
		}

#if defined(HAVE_SIGNAL_H) && defined(HAVE_SETJMP_H)
		if (ctx->thread == 0 && ! RUNNING_ON_VALGRIND) {
		    void (* volatile old_segfault_handler)(int);
		    void (* volatile old_sigpipe_handler)(int);

		    /* Set up a checkpoint to get back to in case of segfaults. */
#ifdef SIGSEGV
		    old_segfault_handler = signal (SIGSEGV, segfault_handler);
#endif
#ifdef SIGPIPE
		    old_sigpipe_handler = signal (SIGPIPE, segfault_handler);
#endif
		    if (0 == setjmp (jmpbuf))
			status = cairo_test_for_target (ctx, target, dev_offset, similar);
		    else
			status = CAIRO_TEST_CRASHED;
#ifdef SIGSEGV
		    signal (SIGSEGV, old_segfault_handler);
#endif
#ifdef SIGPIPE
		    signal (SIGPIPE, old_sigpipe_handler);
#endif
		} else {
		    status = cairo_test_for_target (ctx, target, dev_offset, similar);
		}
#else
		status = cairo_test_for_target (ctx, target, dev_offset, similar);
#endif

		if (ctx->thread == 0) {
		    cairo_test_log (ctx,
			    "TEST: %s TARGET: %s FORMAT: %s OFFSET: %d SIMILAR: %d RESULT: ",
				    ctx->test->name, target->name,
				    cairo_boilerplate_content_name (target->content),
				    dev_offset, similar);
		    switch (status) {
		    case CAIRO_TEST_SUCCESS:
			printf ("PASS\n");
			cairo_test_log (ctx, "PASS\n");
			if (ret == CAIRO_TEST_UNTESTED)
			    ret = CAIRO_TEST_SUCCESS;
			break;
		    case CAIRO_TEST_UNTESTED:
			printf ("UNTESTED\n");
			cairo_test_log (ctx, "UNTESTED\n");
			break;
		    case CAIRO_TEST_CRASHED:
			if (print_fail_on_stdout) {
			    printf ("!!!CRASHED!!!\n");
			} else {
			/* eat the test name */
			printf ("\r");
			fflush (stdout);
			}
			cairo_test_log (ctx, "CRASHED\n");
			fprintf (stderr, "%s-%s-%s [%d]%s:\t%s!!!CRASHED!!!%s\n",
				 ctx->test->name, target->name,
				 cairo_boilerplate_content_name (target->content), dev_offset, similar ? " (similar)" : "",
				 fail_face, normal_face);
			ret = CAIRO_TEST_FAILURE;
			break;
		    default:
		    case CAIRO_TEST_NO_MEMORY:
		    case CAIRO_TEST_FAILURE:
			if (ctx->expectation == CAIRO_TEST_FAILURE) {
			    printf ("XFAIL\n");
			    cairo_test_log (ctx, "XFAIL\n");
			} else {
			    if (print_fail_on_stdout) {
				printf ("FAIL\n");
			    } else {
				/* eat the test name */
				printf ("\r");
				fflush (stdout);
			    }
			    fprintf (stderr, "%s-%s-%s [%d]%s:\t%sFAIL%s\n",
				     ctx->test->name, target->name,
				     cairo_boilerplate_content_name (target->content), dev_offset, similar ? " (similar)" : "",
				     fail_face, normal_face);
			    cairo_test_log (ctx, "FAIL\n");
			}
			ret = CAIRO_TEST_FAILURE;
			break;
		    }
		    fflush (stdout);
		} else {
#if _POSIX_THREAD_SAFE_FUNCTIONS
		    flockfile (stdout);
#endif
		    printf ("%s-%s-%s %d [%d]:\t",
			    ctx->test->name, target->name,
			    cairo_boilerplate_content_name (target->content),
			    ctx->thread,
			    dev_offset);
		    switch (status) {
		    case CAIRO_TEST_SUCCESS:
			printf ("PASS\n");
			break;
		    case CAIRO_TEST_UNTESTED:
			printf ("UNTESTED\n");
			break;
		    case CAIRO_TEST_CRASHED:
			printf ("!!!CRASHED!!!\n");
			ret = CAIRO_TEST_FAILURE;
			break;
		    default:
		    case CAIRO_TEST_NO_MEMORY:
		    case CAIRO_TEST_FAILURE:
			if (ctx->expectation == CAIRO_TEST_FAILURE) {
			    printf ("XFAIL\n");
			} else {
			    printf ("FAIL\n");
			}
			ret = CAIRO_TEST_FAILURE;
			break;
		    }

		    fflush (stdout);
#if _POSIX_THREAD_SAFE_FUNCTIONS
		    funlockfile (stdout);
#endif
		}
	    }
	}
    }

    return ret;
}

#if HAVE_PTHREAD_H
typedef struct _cairo_test_thread {
    pthread_t thread;
    cairo_test_context_t *ctx;
    size_t id;
} cairo_test_thread_t;

static void *
cairo_test_run_threaded (void *closure)
{
    cairo_test_thread_t *arg = closure;
    cairo_test_context_t ctx;
    cairo_test_status_t ret;

    cairo_test_init_thread (&ctx, arg->ctx, arg->id);

    ret = cairo_test_run (&ctx);

    cairo_test_fini (&ctx);

    return (void *) ret;
}
#endif


static cairo_test_status_t
cairo_test_expecting (const cairo_test_t *test,
		      cairo_test_status_t expectation)
{
    cairo_test_context_t ctx;
    cairo_test_status_t ret = CAIRO_TEST_SUCCESS;
    size_t num_threads;

    _cairo_test_init (&ctx, test, test->name, expectation);
    printf ("%s\n", test->description);

    if (expectation == CAIRO_TEST_FAILURE)
	printf ("Expecting failure\n");

#if HAVE_PTHREAD_H
    num_threads = 0;
    if (getenv ("CAIRO_TEST_NUM_THREADS"))
	num_threads = atoi (getenv ("CAIRO_TEST_NUM_THREADS"));
    if (num_threads > 1) {
	cairo_test_thread_t *threads;
	size_t n;

	threads = xmalloc (sizeof (cairo_test_thread_t) * num_threads);
	for (n = 0; n < num_threads; n++) {
	    threads[n].ctx = &ctx;
	    threads[n].id = n + 1;
	    pthread_create (&threads[n].thread, NULL,
		    cairo_test_run_threaded, &threads[n]);
	}
	for (n = 0; n < num_threads; n++) {
	    void *tmp;
	    pthread_join (threads[n].thread, &tmp);
	    if (ret == CAIRO_TEST_SUCCESS)
		ret = (cairo_test_status_t) tmp;
	}
	free (threads);
    }

    if (ret == CAIRO_TEST_SUCCESS)
#endif
	ret = cairo_test_run (&ctx);

    if (ret != CAIRO_TEST_SUCCESS)
        printf ("Check %s%s out for more information.\n", test->name, CAIRO_TEST_LOG_SUFFIX);

    /* if the set of targets to test was limited using CAIRO_TEST_TARGET, we
     * behave slightly differently, to ensure that limiting the targets does
     * not increase the number of tests failing. */
    if (ctx.limited_targets) {
	/* if all passed, but expecting failure, return failure to not
	 * trigger an XPASS failure */
	if (expectation == CAIRO_TEST_FAILURE && ret == CAIRO_TEST_SUCCESS) {
	    printf ("All tested backends passed, but tested targets are manually limited\n"
		    "and the test suite expects this test to fail for at least one target.\n"
		    "Intentionally failing the test, to not fail the suite.\n");
	    ret = CAIRO_TEST_FAILURE;
	}
    }

    cairo_test_fini (&ctx);

    return ret;
}

cairo_test_status_t
cairo_test (const cairo_test_t *test)
{
    cairo_test_status_t expectation = CAIRO_TEST_SUCCESS;
    const char *xfails;

#ifdef _MSC_VER
    /* We don't want an assert dialog, we want stderr */
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

    if ((xfails = getenv ("CAIRO_XFAIL_TESTS")) != NULL) {
	while (*xfails) {
	    const char *end = strpbrk (xfails, " \t\r\n;:,");
	    if (!end)
	        end = xfails + strlen (xfails);

	    if (0 == strncmp (test->name, xfails, end - xfails) &&
		'\0' == test->name[end - xfails]) {
		expectation = CAIRO_TEST_FAILURE;
		break;
	    }

	    if (*end)
	      end++;
	    xfails = end;
	}
    }

    return cairo_test_expecting (test, expectation);
}

const cairo_test_context_t *
cairo_test_get_context (cairo_t *cr)
{
    return cairo_get_user_data (cr, &_cairo_test_context_key);
}

cairo_surface_t *
cairo_test_create_surface_from_png (const cairo_test_context_t *ctx,
	                            const char *filename)
{
    cairo_surface_t *image;
    cairo_status_t status;

    image = cairo_image_surface_create_from_png (filename);
    status = cairo_surface_status (image);
    if (status == CAIRO_STATUS_FILE_NOT_FOUND) {
        /* expect not found when running with srcdir != builddir
         * such as when 'make distcheck' is run
         */
	if (ctx->srcdir) {
	    char *srcdir_filename;
	    xasprintf (&srcdir_filename, "%s/%s", ctx->srcdir, filename);
	    image = cairo_image_surface_create_from_png (srcdir_filename);
	    free (srcdir_filename);
	}
    }

    return image;
}

cairo_pattern_t *
cairo_test_create_pattern_from_png (const cairo_test_context_t *ctx,
	                            const char *filename)
{
    cairo_surface_t *image;
    cairo_pattern_t *pattern;

    image = cairo_test_create_surface_from_png (ctx, filename);

    pattern = cairo_pattern_create_for_surface (image);

    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

    cairo_surface_destroy (image);

    return pattern;
}

static cairo_surface_t *
_draw_check (int width, int height)
{
    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 12, 12);
    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    cairo_set_source_rgb (cr, 0.75, 0.75, 0.75); /* light gray */
    cairo_paint (cr);

    cairo_set_source_rgb (cr, 0.25, 0.25, 0.25); /* dark gray */
    cairo_rectangle (cr, width / 2,  0, width / 2, height / 2);
    cairo_rectangle (cr, 0, height / 2, width / 2, height / 2);
    cairo_fill (cr);

    surface = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);

    return surface;
}

void
cairo_test_paint_checkered (cairo_t *cr)
{
    cairo_surface_t *check;

    check = _draw_check (12, 12);

    cairo_save (cr);
    cairo_set_source_surface (cr, check, 0, 0);
    cairo_surface_destroy (check);

    cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
    cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    cairo_paint (cr);

    cairo_restore (cr);
}

cairo_bool_t
cairo_test_is_target_enabled (const cairo_test_context_t *ctx, const char *target)
{
    size_t i;

    for (i = 0; i < ctx->num_targets; i++) {
	const cairo_boilerplate_target_t *t = ctx->targets_to_test[i];
	if (strcmp (t->name, target) == 0) {
	    /* XXX ask the target whether is it possible to run?
	     * e.g. the xlib backend could check whether it is able to connect
	     * to the Display.
	     */
	    return TRUE;
	}
    }

    return FALSE;
}

cairo_bool_t
cairo_test_malloc_failure (const cairo_test_context_t *ctx,
			   cairo_status_t status)
{
    int n_faults;

    if (! ctx->malloc_failure)
	return FALSE;

    if (status != CAIRO_STATUS_NO_MEMORY)
	return FALSE;

#if HAVE_MEMFAULT
    /* prevent infinite loops... */
    n_faults = VALGRIND_COUNT_FAULTS ();
    if (n_faults == ctx->last_fault_count)
	return FALSE;

    ((cairo_test_context_t *) ctx)->last_fault_count = n_faults;
#endif

    return TRUE;
}

cairo_test_status_t
cairo_test_status_from_status (const cairo_test_context_t *ctx,
			       cairo_status_t status)
{
    if (status == CAIRO_STATUS_SUCCESS)
	return CAIRO_TEST_SUCCESS;

    if (cairo_test_malloc_failure (ctx, status))
	return CAIRO_TEST_NO_MEMORY;

    return CAIRO_TEST_FAILURE;
}
