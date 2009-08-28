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

#ifndef _CAIRO_TEST_H_
#define _CAIRO_TEST_H_

#define CAIRO_BOILERPLATE_LOG(...) cairo_test_log (__VA_ARGS__)
#include "cairo-boilerplate.h"

CAIRO_BEGIN_DECLS

#if   HAVE_STDINT_H
# include <stdint.h>
#elif HAVE_INTTYPES_H
# include <inttypes.h>
#elif HAVE_SYS_INT_TYPES_H
# include <sys/int_types.h>
#elif defined(_MSC_VER)
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
# ifndef HAVE_UINT64_T
#  define HAVE_UINT64_T 1
# endif
#else
#error Cannot find definitions for fixed-width integral types (uint8_t, uint32_t, \etc.)
#endif

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif

#include <math.h>

typedef enum cairo_test_status {
    CAIRO_TEST_SUCCESS = 0,
    CAIRO_TEST_NO_MEMORY,
    CAIRO_TEST_FAILURE,
    CAIRO_TEST_CRASHED,
    CAIRO_TEST_UNTESTED = 77 /* match automake's skipped exit status */
} cairo_test_status_t;

typedef struct _cairo_test_context cairo_test_context_t;
typedef struct _cairo_test cairo_test_t;

typedef cairo_test_status_t  (cairo_test_draw_function_t) (cairo_t *cr, int width, int height);

struct _cairo_test {
    const char *name;
    const char *description;
    int width;
    int height;
    cairo_test_draw_function_t *draw;
};

/* The standard test interface which works by examining result image.
 *
 * cairo_test() accepts a test struct which will be called once for
 * each testable backend. The following checks will be performed for
 * each backend:
 *
 * 1) If draw() does not return CAIRO_TEST_SUCCESS then this backend
 *    fails.
 *
 * 2) Otherwise, if cairo_status(cr) indicates an error then this
 *    backend fails.
 *
 * 3) Otherwise, if the image size is 0, then this backend passes.
 *
 * 4) Otherwise, if every channel of every pixel exactly matches the
 *    reference image then this backend passes. If not, this backend
 *    fails.
 *
 * The overall test result is PASS if and only if there is at least
 * one backend that is tested and if all tested backend pass according
 * to the four criteria above.
 */
cairo_test_status_t
cairo_test (const cairo_test_t *test);

/* The full context for the test.
 * For ordinary tests (using cairo_test()) the context is passed to the draw
 * routine via user_data on the cairo_t.  The reason why the context is not
 * passed as an explicit parameter is that it is rarely required by the test
 * itself and by removing the parameter we can keep the draw routines simple
 * and serve as example code.
 */
struct _cairo_test_context {
    const cairo_test_t *test;
    const char *test_name;
    cairo_test_status_t expectation;

    FILE *log_file;
    const char *srcdir; /* directory containing sources and input data */
    const char *refdir; /* directory containing reference images */

    char *ref_name; /* cache of the current reference image */
    cairo_surface_t *ref_image;
    cairo_surface_t *ref_image_flattened;

    size_t num_targets;
    cairo_bool_t limited_targets;
    cairo_boilerplate_target_t **targets_to_test;

    int malloc_failure;
    int last_fault_count;

    int thread;
};

/* Retrieve the test context from the cairo_t, used for logging, paths etc */
const cairo_test_context_t *
cairo_test_get_context (cairo_t *cr);


/* cairo_test_init(), cairo_test_log(), and cairo_test_fini() exist to
 * help in writing tests for which cairo_test() is not appropriate for
 * one reason or another. For example, some tests might not be doing
 * any drawing at all, or may need to create their own cairo_t rather
 * than be handed one by cairo_test.
 */


/* Initialize test-specific resources, (log files, etc.) */
void
cairo_test_init (cairo_test_context_t *ctx,
		 const char *test_name);

/* Finalize test-specific resource. */
void
cairo_test_fini (cairo_test_context_t *ctx);


/* Print a message to the log file, ala printf. */
void
cairo_test_log (const cairo_test_context_t *ctx,
	        const char *fmt, ...) CAIRO_BOILERPLATE_PRINTF_FORMAT(2, 3);

void
cairo_test_log_path (const cairo_test_context_t *ctx,
		     const cairo_path_t *path);

/* Helper functions that take care of finding source images even when
 * building in a non-srcdir manner, (ie. the tests will be run in a
 * directory that is different from the one where the source image
 * exists). */
cairo_surface_t *
cairo_test_create_surface_from_png (const cairo_test_context_t *ctx,
	                            const char *filename);

cairo_pattern_t *
cairo_test_create_pattern_from_png (const cairo_test_context_t *ctx,
	                            const char *filename);

void
cairo_test_paint_checkered (cairo_t *cr);

#define CAIRO_TEST_DOUBLE_EQUALS(a,b)  (fabs((a)-(b)) < 0.00001)

cairo_bool_t
cairo_test_is_target_enabled (const cairo_test_context_t *ctx,
	                      const char *target);

cairo_bool_t
cairo_test_malloc_failure (const cairo_test_context_t *ctx,
	                   cairo_status_t status);

cairo_test_status_t
cairo_test_status_from_status (const cairo_test_context_t *ctx,
			       cairo_status_t status);

char *
cairo_test_reference_image_filename (const cairo_test_context_t *ctx,
	                             const char *base_name,
				     const char *test_name,
				     const char *target_name,
				     const char *format);

cairo_surface_t *
cairo_test_get_reference_image (cairo_test_context_t *ctx,
				const char *filename,
				cairo_bool_t flatten);

CAIRO_END_DECLS

#endif
