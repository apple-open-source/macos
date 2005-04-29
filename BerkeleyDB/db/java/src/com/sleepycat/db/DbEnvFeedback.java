/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1999-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbEnvFeedback.java,v 1.2 2004/03/30 01:23:37 jtownsen Exp $
 */
package com.sleepycat.db;

/**
 * @deprecated    As of Berkeley DB 4.2, replaced by {@link
 *      DbEnvFeedbackHandler}
 */
public interface DbEnvFeedback {
    /**
     * @param  env
     * @param  opcode
     * @param  percent
     * @deprecated      As of Berkeley DB 4.2, replaced by {@link
     *      DbEnvFeedbackHandler#feedback(DbEnv,int,int)
     *      DbEnvFeedbackHandler.feedback(DbEnv,int,int)}
     */
    public abstract void feedback(DbEnv env, int opcode, int percent);
}
