/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestExceptInclude.cpp,v 1.1 2003/02/15 04:56:19 zarzycki Exp $
 */

/* We should be able to include cxx_except.h without db_cxx.h,
 * and use the DbException class.  We do need db.h to get a few
 * typedefs defined that the DbException classes use.
 *
 * This program does nothing, it's just here to make sure
 * the compilation works.
 */
#include <db.h>
#include <cxx_except.h>

int main(int argc, char *argv[])
{
	DbException *dbe = new DbException("something");
	DbMemoryException *dbme = new DbMemoryException("anything");

	dbe = dbme;
}

