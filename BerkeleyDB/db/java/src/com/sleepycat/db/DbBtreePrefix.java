/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbBtreePrefix.java,v 1.1.1.1 2003/02/15 04:56:07 zarzycki Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_bt_prefix()
 *
 */
public interface DbBtreePrefix
{
    public abstract int bt_prefix(Db db, Dbt dbt1, Dbt dbt2);
}

// end of DbBtreePrefix.java
