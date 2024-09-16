/*
 * Copyright (c) 2015-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <sys/kernel.h>
#include <sys/commpage.h>
#include <sys/kernel_types.h>
#include <sys/persona.h>
#include <pexpert/pexpert.h>
#include <machine/cpu_capabilities.h>

#if CONFIG_PERSONAS
#include <machine/atomic.h>

#include <kern/assert.h>
#include <kern/simple_lock.h>
#include <kern/task.h>
#include <kern/zalloc.h>
#include <mach/thread_act.h>
#include <kern/thread.h>

#include <sys/param.h>
#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/proc_info.h>
#include <sys/resourcevar.h>

#include <security/audit/audit.h>

#include <os/log.h>
#define pna_err(fmt, ...) \
	os_log_error(OS_LOG_DEFAULT, "ERROR: " fmt, ## __VA_ARGS__)

#define MAX_PERSONAS     512

#define TEMP_PERSONA_ID  499

#define FIRST_PERSONA_ID 501
#define PERSONA_ID_STEP   10

#define PERSONA_ALLOC_TOKEN   (0x7a0000ae)
#define PERSONA_INIT_TOKEN    (0x7500005e)
#define PERSONA_MAGIC         (0x0aa55aa0)
#define persona_initialized(p) ((p)->pna_valid == PERSONA_MAGIC || (p)->pna_valid == PERSONA_INIT_TOKEN)
#define persona_valid(p)      ((p)->pna_valid == PERSONA_MAGIC)
#define persona_mkinvalid(p)  ((p)->pna_valid = ~(PERSONA_MAGIC))

static LIST_HEAD(personalist, persona) all_personas = LIST_HEAD_INITIALIZER(all_personas);
static uint32_t g_total_personas;
const uint32_t g_max_personas = MAX_PERSONAS;
static uid_t g_next_persona_id = FIRST_PERSONA_ID;

LCK_GRP_DECLARE(persona_lck_grp, "personas");
LCK_MTX_DECLARE(all_personas_lock, &persona_lck_grp);

os_refgrp_decl(static, persona_refgrp, "persona", NULL);

static KALLOC_TYPE_DEFINE(persona_zone, struct persona, KT_DEFAULT);

#define lock_personas()    lck_mtx_lock(&all_personas_lock)
#define unlock_personas()  lck_mtx_unlock(&all_personas_lock)

extern kern_return_t bank_get_bank_ledger_thread_group_and_persona(void *voucher,
    void *bankledger, void **banktg, uint32_t *persona_id);
void
ipc_voucher_release(void *voucher);

struct persona *
persona_alloc(uid_t id, const char *login, persona_type_t type, char *path, uid_t uid, int *error)
{
	struct persona *persona;
	int err = 0;

	if (!login) {
		pna_err("Must provide a login name for a new persona!");
		if (error) {
			*error = EINVAL;
		}
		return NULL;
	}

	if (type <= PERSONA_INVALID || type > PERSONA_TYPE_MAX) {
		pna_err("Invalid type: %d", type);
		if (error) {
			*error = EINVAL;
		}
		return NULL;
	}

	persona = zalloc_flags(persona_zone, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	if (os_atomic_inc(&g_total_personas, relaxed) > MAX_PERSONAS) {
		/* too many personas! */
		pna_err("too many active personas!");
		err = EBUSY;
		goto out_error;
	}

	strncpy(persona->pna_login, login, sizeof(persona->pna_login) - 1);
	persona_dbg("Starting persona allocation for: '%s'", persona->pna_login);

	LIST_INIT(&persona->pna_members);
	lck_mtx_init(&persona->pna_lock, &persona_lck_grp, LCK_ATTR_NULL);
	os_ref_init(&persona->pna_refcount, &persona_refgrp);

	persona->pna_type = type;
	persona->pna_id = id;
	persona->pna_valid = PERSONA_ALLOC_TOKEN;
	persona->pna_path = path;
	persona->pna_uid = uid;

	/*
	 * NOTE: this persona has not been fully initialized. A subsequent
	 * call to persona_init_begin() followed by persona_init_end() will make
	 * the persona visible to the rest of the system.
	 */
	if (error) {
		*error = 0;
	}
	return persona;

out_error:
	os_atomic_dec(&g_total_personas, relaxed);
	zfree(persona_zone, persona);
	if (error) {
		*error = err;
	}
	return NULL;
}

/**
 * persona_init_begin
 *
 * This function begins initialization of a persona. It first acquires the
 * global persona list lock via lock_personas(), then selects an appropriate
 * persona ID and sets up the persona's credentials. This function *must* be
 * followed by a call to persona_init_end() which will mark the persona
 * structure as valid
 *
 * Conditions:
 *      persona has been allocated via persona_alloc()
 *      nothing locked
 *
 * Returns:
 *      global persona list is locked (even on error)
 */
int
persona_init_begin(struct persona *persona)
{
	struct persona *tmp;
	int err = 0;
	uid_t id;

	if (!persona || (persona->pna_valid != PERSONA_ALLOC_TOKEN)) {
		return EINVAL;
	}

	id = persona->pna_id;

	lock_personas();
try_again:
	if (id == PERSONA_ID_NONE) {
		persona->pna_id = g_next_persona_id;
	}

	persona_dbg("Beginning Initialization of %d:%d (%s)...", id, persona->pna_id, persona->pna_login);

	err = 0;
	LIST_FOREACH(tmp, &all_personas, pna_list) {
		persona_lock(tmp);
		if (id == PERSONA_ID_NONE && tmp->pna_id == persona->pna_id) {
			persona_unlock(tmp);
			/*
			 * someone else manually claimed this ID, and we're
			 * trying to allocate an ID for the caller: try again
			 */
			g_next_persona_id += PERSONA_ID_STEP;
			goto try_again;
		}
		if (strncmp(tmp->pna_login, persona->pna_login, sizeof(tmp->pna_login)) == 0 ||
		    tmp->pna_id == persona->pna_id) {
			persona_unlock(tmp);
			/*
			 * Disallow use of identical login names and re-use
			 * of previously allocated persona IDs
			 */
			err = EEXIST;
			break;
		}
		persona_unlock(tmp);
	}
	if (err) {
		goto out;
	}

	/* if the kernel supplied the persona ID, increment for next time */
	if (id == PERSONA_ID_NONE) {
		g_next_persona_id += PERSONA_ID_STEP;
	}

	persona->pna_valid = PERSONA_INIT_TOKEN;

out:
	if (err != 0) {
		persona_dbg("ERROR:%d while initializing %d:%d (%s)...", err, id, persona->pna_id, persona->pna_login);
		/*
		 * mark the persona with an error so that persona_init_end()
		 * will *not* add it to the global list.
		 */
		persona->pna_id = PERSONA_ID_NONE;
	}

	/*
	 * leave the global persona list locked: it will be
	 * unlocked in a call to persona_init_end()
	 */
	return err;
}

/**
 * persona_init_end
 *
 * This function finalizes the persona initialization by marking it valid and
 * adding it to the global list of personas. After unlocking the global list,
 * the persona will be visible to the reset of the system. The function will
 * only mark the persona valid if the input parameter 'error' is 0.
 *
 * Conditions:
 *      persona is initialized via persona_init_begin()
 *      global persona list is locked via lock_personas()
 *
 * Returns:
 *      global persona list is unlocked
 */
void
persona_init_end(struct persona *persona, int error)
{
	if (persona == NULL) {
		return;
	}

	/*
	 * If the pna_valid member is set to the INIT_TOKEN value, then it has
	 * successfully gone through persona_init_begin(), and we can mark it
	 * valid and make it visible to the rest of the system. However, if
	 * there was an error either during initialization or otherwise, we
	 * need to decrement the global count of personas because this one
	 * will be disposed-of by the callers invocation of persona_put().
	 */
	if (error != 0 || persona->pna_valid == PERSONA_ALLOC_TOKEN) {
		persona_dbg("ERROR:%d after initialization of %d (%s)", error, persona->pna_id, persona->pna_login);
		/* remove this persona from the global count */
		os_atomic_dec(&g_total_personas, relaxed);
	} else if (error == 0 &&
	    persona->pna_valid == PERSONA_INIT_TOKEN) {
		persona->pna_valid = PERSONA_MAGIC;
		LIST_INSERT_HEAD(&all_personas, persona, pna_list);
		persona_dbg("Initialization of %d (%s) Complete.", persona->pna_id, persona->pna_login);
	}

	unlock_personas();
}

static struct persona *
persona_get_locked(struct persona *persona)
{
	os_ref_retain_locked(&persona->pna_refcount);
	return persona;
}

struct persona *
persona_get(struct persona *persona)
{
	struct persona *ret;
	if (!persona) {
		return NULL;
	}
	persona_lock(persona);
	ret = persona_get_locked(persona);
	persona_unlock(persona);

	return ret;
}

struct persona *
proc_persona_get(proc_t p)
{
	proc_lock(p);
	struct persona *persona = persona_get(p->p_persona);
	proc_unlock(p);

	return persona;
}

static void
persona_put_and_unlock(struct persona *persona)
{
	int destroy = 0;

	if (os_ref_release_locked(&persona->pna_refcount) == 0) {
		destroy = 1;
	}
	persona_unlock(persona);

	if (!destroy) {
		return;
	}

	persona_dbg("Destroying persona %s", persona_desc(persona, 0));

	/* remove it from the global list and decrement the count */
	lock_personas();
	persona_lock(persona);
	if (persona_valid(persona)) {
		LIST_REMOVE(persona, pna_list);
		if (os_atomic_dec_orig(&g_total_personas, relaxed) == 0) {
			panic("persona count underflow!");
		}
		persona_mkinvalid(persona);
	}
	if (persona->pna_path != NULL) {
		zfree(ZV_NAMEI, persona->pna_path);
	}
	persona_unlock(persona);
	unlock_personas();

	assert(LIST_EMPTY(&persona->pna_members));
	memset(persona, 0, sizeof(*persona));
	zfree(persona_zone, persona);
}

void
persona_put(struct persona *persona)
{
	if (persona) {
		persona_lock(persona);
		persona_put_and_unlock(persona);
	}
}

uid_t
persona_get_id(struct persona *persona)
{
	if (persona) {
		return persona->pna_id;
	}
	return PERSONA_ID_NONE;
}

struct persona *
persona_lookup(uid_t id)
{
	struct persona *persona, *tmp;

	persona = NULL;

	/*
	 * simple, linear lookup for now: there shouldn't be too many
	 * of these in memory at any given time.
	 */
	lock_personas();
	LIST_FOREACH(tmp, &all_personas, pna_list) {
		persona_lock(tmp);
		if (tmp->pna_id == id && persona_valid(tmp)) {
			persona = persona_get_locked(tmp);
			persona_unlock(tmp);
			break;
		}
		persona_unlock(tmp);
	}
	unlock_personas();

	return persona;
}

struct persona *
persona_lookup_and_invalidate(uid_t id)
{
	struct persona *persona, *entry, *tmp;

	persona = NULL;

	lock_personas();
	LIST_FOREACH_SAFE(entry, &all_personas, pna_list, tmp) {
		persona_lock(entry);
		if (entry->pna_id == id) {
			if (persona_valid(entry)) {
				persona = persona_get_locked(entry);
				assert(persona != NULL);
				LIST_REMOVE(persona, pna_list);
				if (os_atomic_dec_orig(&g_total_personas, relaxed) == 0) {
					panic("persona ref count underflow!");
				}
				persona_mkinvalid(persona);
			}
			persona_unlock(entry);
			break;
		}
		persona_unlock(entry);
	}
	unlock_personas();

	return persona;
}

int
persona_find_by_type(persona_type_t persona_type, struct persona **persona, size_t *plen)
{
	return persona_find_all(NULL, PERSONA_ID_NONE, persona_type, persona, plen);
}

int
persona_find(const char *login, uid_t uid,
    struct persona **persona, size_t *plen)
{
	return persona_find_all(login, uid, PERSONA_INVALID, persona, plen);
}

int
persona_find_all(const char *login, uid_t uid, persona_type_t persona_type,
    struct persona **persona, size_t *plen)
{
	struct persona *tmp;
	int match = 0;
	size_t found = 0;

	if (login) {
		match++;
	}
	if (uid != PERSONA_ID_NONE) {
		match++;
	}
	if ((persona_type > PERSONA_INVALID) && (persona_type <= PERSONA_TYPE_MAX)) {
		match++;
	} else if (persona_type != PERSONA_INVALID) {
		return EINVAL;
	}

	if (match == 0) {
		return EINVAL;
	}

	persona_dbg("Searching with %d parameters (l:\"%s\", u:%d)",
	    match, login, uid);

	lock_personas();
	LIST_FOREACH(tmp, &all_personas, pna_list) {
		int m = 0;
		persona_lock(tmp);
		if (login && strncmp(tmp->pna_login, login, sizeof(tmp->pna_login)) == 0) {
			m++;
		}
		if (uid != PERSONA_ID_NONE && uid == tmp->pna_id) {
			m++;
		}
		if (persona_type != PERSONA_INVALID && persona_type == tmp->pna_type) {
			m++;
		}
		if (m == match) {
			if (persona && *plen > found) {
				persona[found] = persona_get_locked(tmp);
			}
			found++;
		}
#ifdef PERSONA_DEBUG
		if (m > 0) {
			persona_dbg("ID:%d Matched %d/%d, found:%d, *plen:%d",
			    tmp->pna_id, m, match, (int)found, (int)*plen);
		}
#endif
		persona_unlock(tmp);
	}
	unlock_personas();

	*plen = found;
	if (!found) {
		return ESRCH;
	}
	return 0;
}

struct persona *
persona_proc_get(pid_t pid)
{
	proc_t p = proc_find(pid);
	if (!p) {
		return NULL;
	}

	struct persona *persona = proc_persona_get(p);

	proc_rele(p);

	return persona;
}

uid_t
current_persona_get_id(void)
{
	uid_t current_persona_id = PERSONA_ID_NONE;
	ipc_voucher_t voucher;

	thread_get_mach_voucher(current_thread(), 0, &voucher);
	/* returns a voucher ref */
	if (voucher != IPC_VOUCHER_NULL) {
		/*
		 * If the voucher doesn't contain a bank attribute, it uses
		 * the default bank task value to determine the persona id
		 * which is the same as the proc's persona id
		 */
		bank_get_bank_ledger_thread_group_and_persona(voucher, NULL,
		    NULL, &current_persona_id);
		ipc_voucher_release(voucher);
	} else {
		/* Fallback - get the proc's persona */
		current_persona_id = proc_persona_id(current_proc());
	}
	return current_persona_id;
}

struct persona *
current_persona_get(void)
{
	struct persona *persona = NULL;
	uid_t current_persona_id = PERSONA_ID_NONE;

	current_persona_id = current_persona_get_id();
	persona = persona_lookup(current_persona_id);
	return persona;
}

typedef enum e_persona_reset_op {
	PROC_REMOVE_PERSONA = 1,
	PROC_RESET_OLD_PERSONA = 2,
} persona_reset_op_t;

/*
 * internal cleanup routine for proc_set_persona_internal
 *
 */
static struct persona *
proc_reset_persona_internal(proc_t p, persona_reset_op_t op,
    struct persona *old_persona,
    struct persona *new_persona)
{
#if (DEVELOPMENT || DEBUG)
	persona_lock_assert_held(new_persona);
#endif

	switch (op) {
	case PROC_REMOVE_PERSONA:
		old_persona = p->p_persona;
		OS_FALLTHROUGH;
	case PROC_RESET_OLD_PERSONA:
		break;
	default:
		/* invalid arguments */
		return NULL;
	}

	/* unlock the new persona (locked on entry) */
	persona_unlock(new_persona);
	/* lock the old persona and the process */
	assert(old_persona != NULL);
	persona_lock(old_persona);
	proc_lock(p);

	switch (op) {
	case PROC_REMOVE_PERSONA:
		LIST_REMOVE(p, p_persona_list);
		p->p_persona = NULL;
		break;
	case PROC_RESET_OLD_PERSONA:
		p->p_persona = old_persona;
		LIST_INSERT_HEAD(&old_persona->pna_members, p, p_persona_list);
		break;
	}

	proc_unlock(p);
	persona_unlock(old_persona);

	/* re-lock the new persona */
	persona_lock(new_persona);
	return old_persona;
}

/*
 * Assumes persona is locked.
 * On success, takes a reference to 'persona' and returns the
 * previous persona the process had adopted. The caller is
 * responsible to release the reference.
 */
static struct persona *
proc_set_persona_internal(
	proc_t                  p,
	struct persona         *persona,
	kauth_cred_derive_t     derive_fn,
	int                    *rlim_error)
{
	struct persona *old_persona = NULL;
	uid_t old_uid, new_uid;
	size_t count;
	rlim_t nproc = proc_limitgetcur(p, RLIMIT_NPROC);

	/*
	 * This operation must be done under the proc trans lock
	 * by the thread which took the trans lock!
	 */
	assert(((p->p_lflag & P_LINTRANSIT) == P_LINTRANSIT) &&
	    p->p_transholder == current_thread());
	assert(persona != NULL);

	/* no work to do if we "re-adopt" the same persona */
	if (p->p_persona == persona) {
		return NULL;
	}

	/*
	 * If p is in a persona, then we need to remove 'p' from the list of
	 * processes in that persona. To do this, we need to drop the lock
	 * held on the incoming (new) persona and lock the old one.
	 */
	if (p->p_persona) {
		old_persona = proc_reset_persona_internal(p, PROC_REMOVE_PERSONA,
		    NULL, persona);
	}

	/*
	 * Check to see if we will hit a proc rlimit by moving the process
	 * into the persona. If so, we'll bail early before actually moving
	 * the process or changing its credentials.
	 */
	new_uid = persona->pna_id;

	if (new_uid != 0 &&
	    (rlim_t)chgproccnt(new_uid, 0) > nproc) {
		pna_err("PID:%d hit proc rlimit in new persona(%d): %s",
		    proc_getpid(p), new_uid, persona_desc(persona, 1));

		*rlim_error = EACCES;

		if (old_persona) {
			(void)proc_reset_persona_internal(p, PROC_RESET_OLD_PERSONA,
			    old_persona, persona);
		}

		return NULL;
	}

	*rlim_error = 0;

	if (old_persona) {
		old_uid = old_persona->pna_id;
	} else {
		/* proc_ucred_unsafe() is OK because p is a fork/exec/... child */
		old_uid = kauth_cred_getruid(proc_ucred_unsafe(p));
	}

	if (derive_fn) {
		kauth_cred_proc_update(p, PROC_SETTOKEN_SETUGID, derive_fn);
	}

	if (new_uid != old_uid) {
		count = chgproccnt(old_uid, -1);
		persona_dbg("Decrement %s:%d proc_count to: %lu",
		    old_persona ? "Persona" : "UID", old_uid, count);

		/*
		 * Increment the proc count on the UID associated with
		 * the new persona. Enforce the resource limit just
		 * as in fork1()
		 */
		count = chgproccnt(new_uid, 1);
		persona_dbg("Increment Persona:%d proc_count to: %lu",
		    new_uid, count);
	}

	OSBitOrAtomic(P_ADOPTPERSONA, &p->p_flag);

	proc_lock(p);
	p->p_persona = persona_get_locked(persona);
	LIST_INSERT_HEAD(&persona->pna_members, p, p_persona_list);
	proc_unlock(p);

	return old_persona;
}

/* only called during fork or exec: child's ucred is stable */
int
persona_proc_adopt(
	proc_t                  p,
	struct persona         *persona, /* consumed */
	kauth_cred_derive_t     derive_fn)
{
	int error;
	struct persona *old_persona;

	if (!persona) {
		return EINVAL;
	}

	persona_dbg("%d adopting Persona %d (%s)", proc_pid(p),
	    persona->pna_id, persona_desc(persona, 0));

	persona_lock(persona);
	if (!persona_valid(persona)) {
		persona_dbg("Invalid persona (%s)!", persona_desc(persona, 1));
		persona_put_and_unlock(persona);
		return EINVAL;
	}

	/*
	 * assume the persona: this may drop and re-acquire the persona lock!
	 */
	error = 0;
	old_persona = proc_set_persona_internal(p, persona, derive_fn, &error);

	/* Only Multiuser Mode needs to update the session login name to the persona name */
#if XNU_TARGET_OS_IOS || XNU_TARGET_OS_XR
	uint32_t multiuser_flags = COMM_PAGE_READ(uint32_t, MULTIUSER_CONFIG);
	/* set the login name of the session */
	if (multiuser_flags & kIsMultiUserDevice) {
		struct pgrp *pg;
		struct session *sessp;

		if ((pg = proc_pgrp(p, &sessp)) != PGRP_NULL) {
			session_lock(sessp);
			bcopy(persona->pna_login, sessp->s_login, MAXLOGNAME);
			session_unlock(sessp);
			pgrp_rele(pg);
		}
	}
#endif
	persona_put_and_unlock(persona);

	/*
	 * Drop the reference to the old persona.
	 */
	if (old_persona) {
		persona_put(old_persona);
	}

	persona_dbg("%s", error == 0 ? "SUCCESS" : "FAILED");
	return error;
}

int
persona_proc_drop(proc_t p)
{
	struct persona *persona = NULL;

	persona_dbg("PID:%d, %s -> <none>", proc_getpid(p), persona_desc(p->p_persona, 0));

	/*
	 * There are really no other credentials for us to assume,
	 * so we'll just continue running with the credentials
	 * we got from the persona.
	 */

	/*
	 * the locks must be taken in reverse order here, so
	 * we have to be careful not to cause deadlock
	 */
try_again:
	proc_lock(p);
	if (p->p_persona) {
		uid_t puid, ruid;
		if (!persona_try_lock(p->p_persona)) {
			proc_unlock(p);
			mutex_pause(0); /* back-off time */
			goto try_again;
		}
		persona = p->p_persona;
		LIST_REMOVE(p, p_persona_list);
		p->p_persona = NULL;

		smr_proc_task_enter();
		ruid = kauth_cred_getruid(proc_ucred_smr(p));
		smr_proc_task_leave();

		puid = persona->pna_id;
		proc_unlock(p);
		(void)chgproccnt(ruid, 1);
		(void)chgproccnt(puid, -1);
	} else {
		proc_unlock(p);
	}

	/*
	 * if the proc had a persona, then it is still locked here
	 * (preserving proper lock ordering)
	 */

	if (persona) {
		persona_unlock(persona);
		persona_put(persona);
	}

	return 0;
}

int
persona_get_type(struct persona *persona)
{
	int type;

	if (!persona) {
		return PERSONA_INVALID;
	}

	persona_lock(persona);
	if (!persona_valid(persona)) {
		persona_unlock(persona);
		return PERSONA_INVALID;
	}
	type = persona->pna_type;
	persona_unlock(persona);

	return type;
}

uid_t
persona_get_uid(struct persona *persona)
{
	uid_t uid = KAUTH_UID_NONE;

	if (!persona) {
		return KAUTH_UID_NONE;
	}

	persona_lock(persona);
	if (persona_valid(persona)) {
		uid = persona->pna_uid;
	}
	persona_unlock(persona);

	return uid;
}

boolean_t
persona_is_adoption_allowed(struct persona *persona)
{
	if (!persona) {
		return FALSE;
	}
	int type = persona->pna_type;
	return type == PERSONA_SYSTEM || type == PERSONA_SYSTEM_PROXY;
}

#else /* !CONFIG_PERSONAS */

/*
 * symbol exports for kext compatibility
 */

uid_t
persona_get_id(__unused struct persona *persona)
{
	return PERSONA_ID_NONE;
}

uid_t
persona_get_uid(__unused struct persona *persona)
{
	return KAUTH_UID_NONE;
}

int
persona_get_type(__unused struct persona *persona)
{
	return PERSONA_INVALID;
}

struct persona *
persona_lookup(__unused uid_t id)
{
	return NULL;
}

int
persona_find(__unused const char *login,
    __unused uid_t uid,
    __unused struct persona **persona,
    __unused size_t *plen)
{
	return ENOTSUP;
}

int
persona_find_by_type(__unused int persona_type,
    __unused struct persona **persona,
    __unused size_t *plen)
{
	return ENOTSUP;
}

struct persona *
persona_proc_get(__unused pid_t pid)
{
	return NULL;
}

uid_t
current_persona_get_id(void)
{
	return PERSONA_ID_NONE;
}

struct persona *
current_persona_get(void)
{
	return NULL;
}

struct persona *
persona_get(struct persona *persona)
{
	return persona;
}

struct persona *
proc_persona_get(__unused proc_t p)
{
	return NULL;
}

void
persona_put(__unused struct persona *persona)
{
	return;
}

boolean_t
persona_is_adoption_allowed(__unused struct persona *persona)
{
	return FALSE;
}
#endif
