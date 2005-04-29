/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 2000-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbAppendRecno.java,v 1.2 2004/03/30 01:23:36 jtownsen Exp $
 */
package com.sleepycat.db;

/**
 *  An interface specifying a callback function that modifies stored
 *  data based on a generated key.</p>
 */
public interface DbAppendRecno {
    /**
     *  The DbAppendRecno interface is used by the Db.setAppendRecno
     *  method.</p> The called function may modify the data {@link
     *  com.sleepycat.db.Dbt Dbt}. </p>
     *
     * @param  db            the enclosing database handle.
     * @param  data          the data {@link com.sleepycat.db.Dbt Dbt}
     *      to be stored.
     * @param  recno         the generated record number.
     * @throws  DbException  Signals that an exception of some sort
     *      has occurred.
     */
    public abstract void dbAppendRecno(Db db, Dbt data, int recno)
             throws DbException;
}
