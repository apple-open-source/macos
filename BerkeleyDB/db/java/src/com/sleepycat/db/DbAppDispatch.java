/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbAppDispatch.java,v 1.1.1.1 2003/02/15 04:56:07 zarzycki Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_app_dispatch()
 *
 */
public interface DbAppDispatch
{
    // The value of recops is one of the Db.DB_TXN_* constants
    public abstract int app_dispatch(DbEnv env, Dbt dbt, DbLsn lsn, int recops);
}

// end of DbAppDispatch.java
