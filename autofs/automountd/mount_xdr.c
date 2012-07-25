/*
 * This file was generated using rpcgen, and then hand-tweaked to get rid
 * of recursion when processing lists.  In this case, to iterate is human
 * but efficient; to recurse is divine but wasteful.
 */

#include <stdio.h>
#include <stdlib.h>

#include "mount.h"
#ifndef lint
/*static char sccsid[] = "from: @(#)mount.x 1.2 87/09/18 Copyr 1987 Sun Micro";*/
/*static char sccsid[] = "from: @(#)mount.x	2.1 88/08/01 4.0 RPCSRC";*/
/*static char rcsid[] = "from FreeBSD: mount.x,v 1.4 1997/04/18 12:31:26 dfr Exp $";*/
/*static char rcsid[] = "$Id: mount.x,v 1.2 2000/03/05 02:04:44 wsanchez Exp $";*/
#endif /* not lint */

bool_t
xdr_fhandle(xdrs, objp)
	XDR *xdrs;
	fhandle objp;
{

	if (!xdr_opaque(xdrs, (uint8_t *) objp, FHSIZE))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_fhandle3(xdrs, objp)
	XDR *xdrs;
	fhandle3 *objp;
{

	if (!xdr_bytes(xdrs, (uint8_t **)&objp->fhandle3_val, (u_int *)&objp->fhandle3_len, FHSIZE3))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_fhstatus(xdrs, objp)
	XDR *xdrs;
	fhstatus *objp;
{

	if (!xdr_u_int(xdrs, &objp->fhs_status))
		return (FALSE);
	switch (objp->fhs_status) {
	case 0:
		if (!xdr_fhandle(xdrs, objp->fhstatus_u.fhs_fhandle))
			return (FALSE);
		break;
	default:
		break;
	}
	return (TRUE);
}

bool_t
xdr_mountstat3(xdrs, objp)
	XDR *xdrs;
	mountstat3 *objp;
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_mountres3_ok(xdrs, objp)
	XDR *xdrs;
	mountres3_ok *objp;
{

	if (!xdr_fhandle3(xdrs, &objp->fhandle))
		return (FALSE);
	if (!xdr_array(xdrs, (void **)&objp->auth_flavors.auth_flavors_val, (u_int *)&objp->auth_flavors.auth_flavors_len, ~0, sizeof(int), (xdrproc_t)xdr_int))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_mountres3(xdrs, objp)
	XDR *xdrs;
	mountres3 *objp;
{

	if (!xdr_mountstat3(xdrs, &objp->fhs_status))
		return (FALSE);
	switch (objp->fhs_status) {
	case 0:
		if (!xdr_mountres3_ok(xdrs, &objp->mountres3_u.mountinfo))
			return (FALSE);
		break;
	default:
		break;
	}
	return (TRUE);
}

bool_t
xdr_dirpath(xdrs, objp)
	XDR *xdrs;
	dirpath *objp;
{

	if (!xdr_string(xdrs, objp, MNTPATHLEN))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_name(xdrs, objp)
	XDR *xdrs;
	name *objp;
{

	if (!xdr_string(xdrs, objp, MNTNAMLEN))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_mountlist(xdrs, objp)
	XDR *xdrs;
	mountlist *objp;
{
	bool_t more_data;

	switch (xdrs->x_op) {

	case XDR_FREE: {
		mountbody *mb, *tmp;

		tmp = *objp;

		while (tmp != NULL) {
			mb = tmp;
			tmp = mb->ml_next;
			if (!xdr_name(xdrs, &mb->ml_hostname))
				return (FALSE);
			if (!xdr_dirpath(xdrs, &mb->ml_directory))
				return (FALSE);
			free(mb);
		}

		break;
	}

	case XDR_DECODE: {
		mountbody *mb;
		mountbody *mb_prev = NULL;

		*objp = NULL;
		for (;;) {
			if (!xdr_bool(xdrs, &more_data))
				return (FALSE);

			if (!more_data)
				break;

			mb = (mountbody *)malloc(sizeof (struct mountbody));
			if (mb == NULL) {
				fprintf(stderr,
				    "xdr_mountlist: out of memory\n");
				return (FALSE);
			}
			mb->ml_hostname = NULL;
			mb->ml_directory = NULL;
			mb->ml_next = NULL;

			if (mb_prev == NULL) {
				mb_prev = mb;
				*objp = mb;
			}

			if (!xdr_name(xdrs, &mb->ml_hostname))
				return (FALSE);
			if (!xdr_dirpath(xdrs, &mb->ml_directory))
				return (FALSE);

			if (mb_prev != mb) {
				mb_prev->ml_next = mb;
				mb_prev = mb;
			}
		}
		break;
	}

	case XDR_ENCODE: {
		mountbody *mb;

		mb = *objp;

		for (;;) {
			more_data = mb != NULL;

			if (!xdr_bool(xdrs, &more_data))
				return (FALSE);

			if (!more_data)
				break;

			if (!xdr_name(xdrs, &mb->ml_hostname))
				return (FALSE);
			if (!xdr_dirpath(xdrs, &mb->ml_directory))
				return (FALSE);

			mb = mb->ml_next;
		}
		break;
	}

	default:
		break;
	}

	return (TRUE);
}

bool_t
xdr_groups(xdrs, objp)
	XDR *xdrs;
	groups *objp;
{

	bool_t more_data;

	switch (xdrs->x_op) {

	case XDR_FREE: {
		groupnode *gn, *tmp;

		tmp = *objp;

		while (tmp != NULL) {
			gn = tmp;
			tmp = gn->gr_next;
			if (!xdr_name(xdrs, &gn->gr_name))
				return (FALSE);
			free(gn);
		}

		break;
	}

	case XDR_DECODE: {
		groupnode *gn;
		groupnode *gn_prev = NULL;

		*objp = NULL;
		for (;;) {
			if (!xdr_bool(xdrs, &more_data))
				return (FALSE);

			if (!more_data)
				break;

			gn = (groupnode *)malloc(sizeof (struct groupnode));
			if (gn == NULL) {
				fprintf(stderr,
				    "xdr_groups: out of memory\n");
				return (FALSE);
			}
			gn->gr_name = NULL;
			gn->gr_next = NULL;

			if (gn_prev == NULL) {
				gn_prev = gn;
				*objp = gn;
			}

			if (!xdr_name(xdrs, &gn->gr_name))
				return (FALSE);

			if (gn_prev != gn) {
				gn_prev->gr_next = gn;
				gn_prev = gn;
			}
		}
		break;
	}

	case XDR_ENCODE: {
		groupnode *gn;

		gn = *objp;

		for (;;) {
			more_data = gn != NULL;

			if (!xdr_bool(xdrs, &more_data))
				return (FALSE);

			if (!more_data)
				break;

			if (!xdr_name(xdrs, &gn->gr_name))
				return (FALSE);

			gn = gn->gr_next;
		}
		break;
	}

	default:
		break;
	}

	return (TRUE);
}

bool_t
xdr_exports(xdrs, objp)
	XDR *xdrs;
	exports *objp;
{

	bool_t more_data;

	switch (xdrs->x_op) {

	case XDR_FREE: {
		exportnode *en, *tmp;

		tmp = *objp;

		while (tmp != NULL) {
			en = tmp;
			tmp = en->ex_next;
			if (!xdr_dirpath(xdrs, &en->ex_dir))
				return (FALSE);
			if (!xdr_groups(xdrs, &en->ex_groups))
				return (FALSE);
			free(en);
		}

		break;
	}

	case XDR_DECODE: {
		exportnode *en;
		exportnode *en_prev = NULL;

		*objp = NULL;
		for (;;) {
			if (!xdr_bool(xdrs, &more_data))
				return (FALSE);

			if (!more_data)
				break;

			en = (exportnode *)malloc(sizeof (struct exportnode));
			if (en == NULL) {
				fprintf(stderr,
				    "xdr_exports: out of memory\n");
				return (FALSE);
			}
			en->ex_dir = NULL;
			en->ex_groups = NULL;
			en->ex_next = NULL;

			if (en_prev == NULL) {
				en_prev = en;
				*objp = en;
			}

			if (!xdr_dirpath(xdrs, &en->ex_dir))
				return (FALSE);
			if (!xdr_groups(xdrs, &en->ex_groups))
				return (FALSE);

			if (en_prev != en) {
				en_prev->ex_next = en;
				en_prev = en;
			}
		}
		break;
	}

	case XDR_ENCODE: {
		exportnode *en;

		en = *objp;

		for (;;) {
			more_data = en != NULL;

			if (!xdr_bool(xdrs, &more_data))
				return (FALSE);

			if (!more_data)
				break;

			if (!xdr_dirpath(xdrs, &en->ex_dir))
				return (FALSE);
			if (!xdr_groups(xdrs, &en->ex_groups))
				return (FALSE);

			en = en->ex_next;
		}
		break;
	}

	default:
		break;
	}

	return (TRUE);
}
