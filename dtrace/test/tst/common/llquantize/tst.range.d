/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

#pragma D option quiet

BEGIN
{
	@["Screven"] = llquantize(0, 10, 1, 2, 20, 25);
	@["Katz"] = llquantize(1, 10, 1, 2, 20, -100);
	@["Kurian"] = llquantize(7, 10, 1, 2, 20, 15);
	@["Rozwat"] = llquantize(49, 10, 1, 2, 20, 15);
	@["Fowler"] = llquantize(343, 10, 1, 2, 20, 150);

	printa(@);
	exit(0);
}
