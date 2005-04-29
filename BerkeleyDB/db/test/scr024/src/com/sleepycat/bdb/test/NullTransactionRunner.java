/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: NullTransactionRunner.java,v 1.2 2004/03/30 01:24:45 jtownsen Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.db.DbEnv;
import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.util.ExceptionUnwrapper;

class NullTransactionRunner extends TransactionRunner {

    NullTransactionRunner(DbEnv env) {

        super(env);
    }

    public void run(TransactionWorker worker)
        throws Exception {

        try {
            worker.doWork();
        } catch (Exception e) {
            throw ExceptionUnwrapper.unwrap(e);
        }
    }
}
