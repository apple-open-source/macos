/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1999-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbDeadlockException.java,v 1.2 2004/03/30 01:23:36 jtownsen Exp $
 */
package com.sleepycat.db;

/**
 *  This information describes the DbDeadlockException class and how
 *  it is used in the Berkeley DB library.</p> <p>
 *
 *  A DbDeadlockException is thrown when multiple threads competing
 *  for a lock are deadlocked, when a lock request has timed out, or
 *  when a lock request would need to block and the transaction has
 *  been configured to not wait for locks. One of the threads'
 *  transactions is selected for termination, and a
 *  DbDeadlockException is thrown to that thread.</p>
 */
public class DbDeadlockException extends DbException {
    /**
     *  Constructor for the DbDeadlockException object
     *
     */
    protected DbDeadlockException(String s, int errno, DbEnv dbenv) {
        super(s, errno, dbenv);
    }
}
