/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbDupCompare.java,v 1.1.1.1 2003/02/15 04:56:07 zarzycki Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_dup_compare()
 *
 */
public interface DbDupCompare
{
    public abstract int dup_compare(Db db, Dbt dbt1, Dbt dbt2);
}

// end of DbDupCompare.java
