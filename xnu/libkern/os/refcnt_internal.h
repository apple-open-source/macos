#ifndef _OS_REFCNT_INTERNAL_H
#define _OS_REFCNT_INTERNAL_H

struct os_refcnt {
	os_ref_atomic_t ref_count;
#if OS_REFCNT_DEBUG
	struct os_refgrp *ref_group;
#endif
};

#if OS_REFCNT_DEBUG

__options_closed_decl(os_refgrp_flags_t, uint64_t, {
	OS_REFGRP_F_NONE           = 0x0,
	OS_REFGRP_F_ALWAYS_ENABLED = 0x1,
});

struct os_refgrp {
	const char *grp_name;
	os_ref_atomic_t grp_children;  /* number of refcount objects in group */
	os_ref_atomic_t grp_count;     /* current reference count of group */
	_Atomic uint64_t grp_retain_total;
	_Atomic uint64_t grp_release_total;
	struct os_refgrp *grp_parent;
	void *grp_log;                 /* refcount logging context */
	uint64_t grp_flags;            /* Unused for now. */
};

#endif

# define OS_REF_ATOMIC_INITIALIZER 0
#if OS_REFCNT_DEBUG
# define OS_REF_INITIALIZER { .ref_count = OS_REF_ATOMIC_INITIALIZER, .ref_group = NULL }
#else
# define OS_REF_INITIALIZER { .ref_count = OS_REF_ATOMIC_INITIALIZER }
#endif

__BEGIN_DECLS

#if OS_REFCNT_DEBUG
# define os_ref_if_debug(x, y) x
#else
# define os_ref_if_debug(x, y) y
#endif

void os_ref_init_count_external(os_ref_atomic_t *, struct os_refgrp *, os_ref_count_t);
void os_ref_retain_external(os_ref_atomic_t *, struct os_refgrp *);
void os_ref_retain_locked_external(os_ref_atomic_t *, struct os_refgrp *);
os_ref_count_t os_ref_release_external(os_ref_atomic_t *, struct os_refgrp *,
    memory_order release_order, memory_order dealloc_order);
os_ref_count_t os_ref_release_relaxed_external(os_ref_atomic_t *, struct os_refgrp *);
os_ref_count_t os_ref_release_barrier_external(os_ref_atomic_t *, struct os_refgrp *);
os_ref_count_t os_ref_release_locked_external(os_ref_atomic_t *, struct os_refgrp *);
bool os_ref_retain_try_external(os_ref_atomic_t *, struct os_refgrp *);

#if XNU_KERNEL_PRIVATE
void os_ref_init_count_internal(os_ref_atomic_t *, struct os_refgrp *, os_ref_count_t);
void os_ref_retain_internal(os_ref_atomic_t *, struct os_refgrp *);
void os_ref_retain_floor_internal(os_ref_atomic_t *, os_ref_count_t, struct os_refgrp *);
os_ref_count_t os_ref_release_relaxed_internal(os_ref_atomic_t *, struct os_refgrp *);
os_ref_count_t os_ref_release_barrier_internal(os_ref_atomic_t *, struct os_refgrp *);
os_ref_count_t os_ref_release_internal(os_ref_atomic_t *, struct os_refgrp *,
    memory_order release_order, memory_order dealloc_order);
bool os_ref_retain_try_internal(os_ref_atomic_t *, struct os_refgrp *);
bool os_ref_retain_floor_try_internal(os_ref_atomic_t *, os_ref_count_t, struct os_refgrp *);
void os_ref_retain_locked_internal(os_ref_atomic_t *, struct os_refgrp *);
void os_ref_retain_floor_locked_internal(os_ref_atomic_t *, os_ref_count_t, struct os_refgrp *);
os_ref_count_t os_ref_release_locked_internal(os_ref_atomic_t *, struct os_refgrp *);
#else
/* For now, the internal and external variants are identical */
#define os_ref_init_count_internal      os_ref_init_count_external
#define os_ref_retain_internal          os_ref_retain_external
#define os_ref_retain_locked_internal   os_ref_retain_locked_external
#define os_ref_release_internal         os_ref_release_external
#define os_ref_release_barrier_internal os_ref_release_barrier_external
#define os_ref_release_relaxed_internal os_ref_release_relaxed_external
#define os_ref_release_locked_internal  os_ref_release_locked_external
#define os_ref_retain_try_internal      os_ref_retain_try_external
#endif

static inline void
os_ref_init_count(struct os_refcnt *rc, struct os_refgrp * __unused grp, os_ref_count_t count)
{
#if OS_REFCNT_DEBUG
	rc->ref_group = grp;
#endif
	os_ref_init_count_internal(&rc->ref_count, os_ref_if_debug(rc->ref_group, NULL), count);
}

static inline void
os_ref_retain(struct os_refcnt *rc)
{
	os_ref_retain_internal(&rc->ref_count, os_ref_if_debug(rc->ref_group, NULL));
}

static inline os_ref_count_t
os_ref_release_locked(struct os_refcnt *rc)
{
	return os_ref_release_locked_internal(&rc->ref_count, os_ref_if_debug(rc->ref_group, NULL));
}

static inline void
os_ref_retain_locked(struct os_refcnt *rc)
{
	os_ref_retain_internal(&rc->ref_count, os_ref_if_debug(rc->ref_group, NULL));
}

static inline bool
os_ref_retain_try(struct os_refcnt *rc)
{
	return os_ref_retain_try_internal(&rc->ref_count, os_ref_if_debug(rc->ref_group, NULL));
}

__deprecated_msg("inefficient codegen, prefer os_ref_release / os_ref_release_relaxed")
static inline os_ref_count_t OS_WARN_RESULT
os_ref_release_explicit(struct os_refcnt *rc, memory_order release_order, memory_order dealloc_order)
{
	return os_ref_release_internal(&rc->ref_count, os_ref_if_debug(rc->ref_group, NULL),
	           release_order, dealloc_order);
}

#if OS_REFCNT_DEBUG
# define os_refgrp_initializer(name, parent, flags) \
	 { \
	        .grp_name =          (name), \
	        .grp_children =      (0u), \
	        .grp_count =         (0u), \
	        .grp_retain_total =  (0u), \
	        .grp_release_total = (0u), \
	        .grp_parent =        (parent), \
	        .grp_log =           NULL, \
	        .grp_flags =         flags, \
	}

# define os_refgrp_decl_flags(qual, var, name, parent, flags) \
	qual struct os_refgrp __attribute__((section("__DATA,__refgrps"))) var =  \
	    os_refgrp_initializer(name, parent, flags)

# define os_refgrp_decl(qual, var, name, parent) \
	os_refgrp_decl_flags(qual, var, name, parent, OS_REFGRP_F_NONE)

# define os_refgrp_decl_extern(var) \
	extern struct os_refgrp var

/* Create a default group based on the init() callsite if no explicit group
 * is provided. */
# define os_ref_init_count(rc, grp, count) ({ \
	        os_refgrp_decl(static, __grp, __func__, NULL); \
	        (os_ref_init_count)((rc), (grp) ? (grp) : &__grp, (count)); \
	})

#else /* OS_REFCNT_DEBUG */

# define os_refgrp_decl(qual, var, name, parent) extern struct os_refgrp var __attribute__((unused))
# define os_refgrp_decl_extern(var) os_refgrp_decl(, var, ,)
# define os_ref_init_count(rc, grp, count) (os_ref_init_count)((rc), NULL, (count))

#endif /* OS_REFCNT_DEBUG */

#if XNU_KERNEL_PRIVATE
void os_ref_panic_live(void *rc) __abortlike;
#else
__abortlike
static inline void
os_ref_panic_live(void *rc)
{
	panic("os_refcnt: unexpected release of final reference (rc=%p)\n", rc);
	__builtin_unreachable();
}
#endif

static inline os_ref_count_t OS_WARN_RESULT
os_ref_release(struct os_refcnt *rc)
{
	return os_ref_release_barrier_internal(&rc->ref_count,
	           os_ref_if_debug(rc->ref_group, NULL));
}

static inline os_ref_count_t OS_WARN_RESULT
os_ref_release_relaxed(struct os_refcnt *rc)
{
	return os_ref_release_relaxed_internal(&rc->ref_count,
	           os_ref_if_debug(rc->ref_group, NULL));
}

static inline void
os_ref_release_live(struct os_refcnt *rc)
{
	if (__improbable(os_ref_release(rc) == 0)) {
		os_ref_panic_live(rc);
	}
}

static inline os_ref_count_t
os_ref_get_count_internal(os_ref_atomic_t *rc)
{
	return atomic_load_explicit(rc, memory_order_relaxed);
}

static inline os_ref_count_t
os_ref_get_count(struct os_refcnt *rc)
{
	return os_ref_get_count_internal(&rc->ref_count);
}

#if !OS_REFCNT_DEBUG
#define os_pcpu_ref_init(ref, grp)              (os_pcpu_ref_init)(ref, NULL)
#define os_pcpu_ref_destroy(ref, grp)           (os_pcpu_ref_destroy)(ref, NULL)
#define os_pcpu_ref_kill(ref, grp)              (os_pcpu_ref_kill)(ref, NULL)
#define os_pcpu_ref_retain(ref, grp)            (os_pcpu_ref_retain)(ref, NULL)
#define os_pcpu_ref_retain_try(ref, grp)        (os_pcpu_ref_retain_try)(ref, NULL)
#define os_pcpu_ref_release(ref, grp)           (os_pcpu_ref_release)(ref, NULL)
#define os_pcpu_ref_release_live(ref, grp)      (os_pcpu_ref_release_live)(ref, NULL)
#endif

#if XNU_KERNEL_PRIVATE
#pragma GCC visibility push(hidden)

/*
 * Raw API
 */

static inline void
os_ref_init_count_raw(os_ref_atomic_t *rc, struct os_refgrp *grp, os_ref_count_t count)
{
	os_ref_init_count_internal(rc, grp, count);
}

static inline void
os_ref_retain_floor(struct os_refcnt *rc, os_ref_count_t f)
{
	os_ref_retain_floor_internal(&rc->ref_count, f, os_ref_if_debug(rc->ref_group, NULL));
}

static inline void
os_ref_retain_raw(os_ref_atomic_t *rc, struct os_refgrp *grp)
{
	os_ref_retain_internal(rc, grp);
}

static inline void
os_ref_retain_floor_raw(os_ref_atomic_t *rc, os_ref_count_t f, struct os_refgrp *grp)
{
	os_ref_retain_floor_internal(rc, f, grp);
}

static inline os_ref_count_t
os_ref_release_raw(os_ref_atomic_t *rc, struct os_refgrp *grp)
{
	return os_ref_release_barrier_internal(rc, grp);
}

static inline os_ref_count_t
os_ref_release_raw_relaxed(os_ref_atomic_t *rc, struct os_refgrp *grp)
{
	return os_ref_release_relaxed_internal(rc, grp);
}

static inline void
os_ref_release_live_raw(os_ref_atomic_t *rc, struct os_refgrp *grp)
{
	if (__improbable(os_ref_release_barrier_internal(rc, grp) == 0)) {
		os_ref_panic_live(rc);
	}
}

static inline bool
os_ref_retain_try_raw(os_ref_atomic_t *rc, struct os_refgrp *grp)
{
	return os_ref_retain_try_internal(rc, grp);
}

static inline bool
os_ref_retain_floor_try_raw(os_ref_atomic_t *rc, os_ref_count_t f,
    struct os_refgrp *grp)
{
	return os_ref_retain_floor_try_internal(rc, f, grp);
}

static inline void
os_ref_retain_locked_raw(os_ref_atomic_t *rc, struct os_refgrp *grp)
{
	os_ref_retain_locked_internal(rc, grp);
}

static inline void
os_ref_retain_floor_locked_raw(os_ref_atomic_t *rc, os_ref_count_t f,
    struct os_refgrp *grp)
{
	os_ref_retain_floor_locked_internal(rc, f, grp);
}

static inline os_ref_count_t
os_ref_release_locked_raw(os_ref_atomic_t *rc, struct os_refgrp *grp)
{
	return os_ref_release_locked_internal(rc, grp);
}

static inline os_ref_count_t
os_ref_get_count_raw(os_ref_atomic_t *rc)
{
	return os_ref_get_count_internal(rc);
}

#if !OS_REFCNT_DEBUG
/* remove the group argument for non-debug */
#define os_ref_init_count_raw(rc, grp, count) (os_ref_init_count_raw)((rc), NULL, (count))
#define os_ref_retain_raw(rc, grp) (os_ref_retain_raw)((rc), NULL)
#define os_ref_retain_floor_raw(rc, f, grp) (os_ref_retain_floor_raw)((rc), f, NULL)
#define os_ref_release_raw(rc, grp) (os_ref_release_raw)((rc), NULL)
#define os_ref_release_raw_relaxed(rc, grp) (os_ref_release_raw_relaxed)((rc), NULL)
#define os_ref_release_live_raw(rc, grp) (os_ref_release_live_raw)((rc), NULL)
#define os_ref_retain_try_raw(rc, grp) (os_ref_retain_try_raw)((rc), NULL)
#define os_ref_retain_floor_try_raw(rc, f, grp) (os_ref_retain_floor_try_raw)((rc), f, NULL)
#define os_ref_retain_locked_raw(rc, grp) (os_ref_retain_locked_raw)((rc), NULL)
#define os_ref_retain_floor_locked_raw(rc, f, grp) (os_ref_retain_floor_locked_raw)((rc), f, NULL)
#define os_ref_release_locked_raw(rc, grp) (os_ref_release_locked_raw)((rc), NULL)
#endif

extern void
os_ref_log_fini(struct os_refgrp *grp);

extern void
os_ref_log_init(struct os_refgrp *grp);

extern void
os_ref_retain_mask_internal(os_ref_atomic_t *rc, uint32_t n, struct os_refgrp *grp);
extern void
os_ref_retain_acquire_mask_internal(os_ref_atomic_t *rc, uint32_t n, struct os_refgrp *grp);
extern uint32_t
os_ref_retain_try_mask_internal(os_ref_atomic_t *, uint32_t n,
    uint32_t reject_mask, struct os_refgrp *grp) OS_WARN_RESULT;
extern bool
os_ref_retain_try_acquire_mask_internal(os_ref_atomic_t *, uint32_t n,
    uint32_t reject_mask, struct os_refgrp *grp) OS_WARN_RESULT;

extern uint32_t
os_ref_release_barrier_mask_internal(os_ref_atomic_t *rc, uint32_t n, struct os_refgrp *grp);
extern uint32_t
os_ref_release_relaxed_mask_internal(os_ref_atomic_t *rc, uint32_t n, struct os_refgrp *grp);

static inline uint32_t
os_ref_get_raw_mask(os_ref_atomic_t *rc)
{
	return os_ref_get_count_internal(rc);
}

static inline uint32_t
os_ref_get_bits_mask(os_ref_atomic_t *rc, uint32_t b)
{
	return os_ref_get_raw_mask(rc) & ((1u << b) - 1);
}

static inline os_ref_count_t
os_ref_get_count_mask(os_ref_atomic_t *rc, uint32_t b)
{
	return os_ref_get_raw_mask(rc) >> b;
}

static inline void
os_ref_retain_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	os_ref_retain_mask_internal(rc, 1u << b, grp);
}

static inline void
os_ref_retain_acquire_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	os_ref_retain_acquire_mask_internal(rc, 1u << b, grp);
}

static inline uint32_t
os_ref_retain_try_mask(os_ref_atomic_t *rc, uint32_t b,
    uint32_t reject_mask, struct os_refgrp *grp)
{
	return os_ref_retain_try_mask_internal(rc, 1u << b, reject_mask, grp);
}

static inline bool
os_ref_retain_try_acquire_mask(os_ref_atomic_t *rc, uint32_t b,
    uint32_t reject_mask, struct os_refgrp *grp)
{
	return os_ref_retain_try_acquire_mask_internal(rc, 1u << b, reject_mask, grp);
}

static inline uint32_t
os_ref_release_raw_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	return os_ref_release_barrier_mask_internal(rc, 1u << b, grp);
}

static inline uint32_t
os_ref_release_raw_relaxed_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	return os_ref_release_relaxed_mask_internal(rc, 1u << b, grp);
}

static inline os_ref_count_t
os_ref_release_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	return os_ref_release_barrier_mask_internal(rc, 1u << b, grp) >> b;
}

static inline os_ref_count_t
os_ref_release_relaxed_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	return os_ref_release_relaxed_mask_internal(rc, 1u << b, grp) >> b;
}

static inline uint32_t
os_ref_release_live_raw_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	uint32_t val = os_ref_release_barrier_mask_internal(rc, 1u << b, grp);
	if (__improbable(val < 1u << b)) {
		os_ref_panic_live(rc);
	}
	return val;
}

static inline void
os_ref_release_live_mask(os_ref_atomic_t *rc, uint32_t b, struct os_refgrp *grp)
{
	os_ref_release_live_raw_mask(rc, b, grp);
}

#if !OS_REFCNT_DEBUG
/* remove the group argument for non-debug */
#define os_ref_init_count_mask(rc, b, grp, init_c, init_b) (os_ref_init_count_mask)(rc, b, NULL, init_c, init_b)
#define os_ref_retain_mask(rc, b, grp) (os_ref_retain_mask)((rc), (b), NULL)
#define os_ref_retain_acquire_mask(rc, b, grp) (os_ref_retain_acquire_mask)((rc), (b), NULL)
#define os_ref_retain_try_mask(rc, b, m, grp) (os_ref_retain_try_mask)((rc), (b), (m), NULL)
#define os_ref_retain_try_acquire_mask(rc, b, grp) (os_ref_retain_try_acquire_mask)((rc), (b), NULL)
#define os_ref_release_mask(rc, b, grp) (os_ref_release_mask)((rc), (b), NULL)
#define os_ref_release_relaxed_mask(rc, b, grp) (os_ref_release_relaxed_mask)((rc), (b), NULL)
#define os_ref_release_raw_mask(rc, b, grp) (os_ref_release_raw_mask)((rc), (b), NULL)
#define os_ref_release_relaxed_raw_mask(rc, b, grp) (os_ref_release_relaxed_raw_mask)((rc), (b), NULL)
#define os_ref_release_live_raw_mask(rc, b, grp) (os_ref_release_live_raw_mask)((rc), (b), NULL)
#define os_ref_release_live_mask(rc, b, grp) (os_ref_release_live_mask)((rc), (b), NULL)
#endif

#pragma GCC visibility pop
#endif

__END_DECLS

#endif /* _OS_REFCNT_INTERNAL_H */
