/*
 * @OSF_COPYRIGHT@
 */

#include <string.h>	/* To get NULL */
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/rpc.h>

#if	0
#	include <stdio.h>
#	define debug(x) printf x
#else
#	define debug(x)
#endif

/*
 *	Routine:	mach_subsystem_join
 *	Purpose:
 *		Create a new subsystem, suitable for registering with
 *		mach_subsystem_create, that consists of the union of
 * 		the routines of subsys_1 and subsys_2.
 *
 *	Warning:
 *		If there is a big gap between the routine numbers of
 *		the two subsystems, a correspondingly large amount of
 *		space will be wasted in the new subsystem.
 */
rpc_subsystem_t
mach_subsystem_join(rpc_subsystem_t subsys_1,	/* First input subsystem */
		    rpc_subsystem_t subsys_2,	/* Second input subsystem */
		    unsigned int *num_bytes_p,	/* Size of output subsystem */
		    void *(*malloc_func)(int)	/* Allocation routine to use */
		)
{
	rpc_subsystem_t			sp, subsys_new;
	int				num_routines, num_args, num_bytes;
	int				i, j;
	struct routine_arg_descriptor	*ap;
	struct routine_descriptor	*rp;

	/* Make sure the two routine number ranges do not overlap:
	 */
	if (subsys_1->start <= subsys_2->start && subsys_1->end > subsys_2->start
					||
	    subsys_2->start <= subsys_1->start && subsys_2->end > subsys_1->start)
		return NULL;

	/* Arrange that subsys_1 is the subsystem with the lower numbered
	 * routines:
	 */
	if (subsys_2->start < subsys_1->start ||
					subsys_2->end < subsys_1->end) {
		/* Exchange the two input subsystem pointers: */
		sp = subsys_2; subsys_2 = subsys_1; subsys_1 = sp;
	}

	debug(("subys_join: Lower subsys: (%d, %d); Higher subsys: (%d, %d)\n",
		subsys_1->start, subsys_1->end, subsys_2->start, subsys_2->end));

	/*
	 * Calculate size needed for new subsystem and allocate it:
	 */
	num_args = 0;
	sp = subsys_1;
	do {
		int	nr;

		nr = sp->end - sp->start;
		num_routines += nr;

		for (rp = &sp->routine[0]; rp < &sp->routine[nr]; rp++) {
			/* Make sure this routine is non-null: */
			if (rp->impl_routine != NULL)
				num_args += rp->descr_count;
		}
		if (sp == subsys_2)
			break;
		sp = subsys_2;
	} while (1);
	num_routines = subsys_2->end - subsys_1->start;

	/* A struct rpc_subsystem, which is just a template for a real
	 * subsystem descriptor, has one dummy routine descriptor in it
	 * and one arg descriptor, so we have to subtract these out, when
	 * calculating room for the routine and arg arrays:
	 */
	num_bytes = sizeof(struct rpc_subsystem) +
		    (num_routines - 1) * sizeof(struct routine_descriptor) +
		    (num_args - 1) * sizeof(struct routine_arg_descriptor);

	debug(("subys_new: %x; #routines: %d; #args: %d; #bytes: %d\n",
			    subsys_new, num_routines, num_args, num_bytes));

	subsys_new = (rpc_subsystem_t) (*malloc_func)(num_bytes);
	if (subsys_new == NULL)
		return NULL;

	/* Initialize the new subsystem, then copy the lower-numbered
	 * subsystem into the new subsystem, then the higher-numbered one:
	 */

	subsys_new->subsystem = NULL;	/* Reserved for system use */
	subsys_new->start = subsys_1->start;
	subsys_new->end = subsys_2->end;
	subsys_new->maxsize = subsys_1->maxsize > subsys_2->maxsize ?
				subsys_1->maxsize : subsys_2->maxsize;
	subsys_new->base_addr = (vm_address_t)subsys_new;

	/* Point ap at the beginning of the arg_descriptors for the
	 * joined subystem, i.e. just after the end of the combined
	 * array of routine descriptors:
	 */
	ap = (struct routine_arg_descriptor *)
				&(subsys_new->routine[num_routines]);
	rp = &(subsys_new->routine[0]);

	/* Copy subsys_1 into subsys_new: */
	debug(("subys_join: Copying lower subsys: rp=%x, ap=%x\n", rp, ap));
	for (i = 0; i < subsys_1->end - subsys_1->start; i++, rp++) {
		*rp = subsys_1->routine[i];
		if (rp->impl_routine != NULL) {
			rp->arg_descr = ap;
			for (j = 0; j < rp->descr_count; j++)
				*ap++ = subsys_1->routine[i].arg_descr[j];
		} else
			rp->arg_descr = NULL;
	}

	/* Fill in the gap, if any, between subsys_1 routine numbers
	 * and subsys_2 routine numbers:
	 */
	for (i = subsys_1->end; i < subsys_2->start; i++, rp++) {
		rp->impl_routine = NULL;
		rp->arg_descr = NULL;
	}

	/* Copy subsys_2 into subsys_new: */
	debug(("subys_join: Copying higher subsys: rp=%x, ap=%x\n", rp, ap));
	for (i = 0; i < subsys_2->end - subsys_2->start; i++, rp++) {
		*rp = subsys_2->routine[i];
		if (rp->impl_routine != NULL) {
			rp->arg_descr = ap;
			for (j = 0; j < rp->descr_count; j++)
				*ap++ = subsys_2->routine[i].arg_descr[j];
		} else
			rp->arg_descr = NULL;
	}
	debug(("subys_join: Done: rp=%x, ap=%x\n", rp, ap));

	*num_bytes_p = num_bytes;
	return subsys_new;
}
