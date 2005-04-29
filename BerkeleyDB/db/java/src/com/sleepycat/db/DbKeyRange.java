/*
 *  -
 *  See the file LICENSE for redistribution information.
 *
 *  Copyright (c) 1997-2003
 *  Sleepycat Software.  All rights reserved.
 *
 *  $Id: DbKeyRange.java,v 1.2 2004/03/30 01:23:37 jtownsen Exp $
 */
package com.sleepycat.db;

/**
 */
public class DbKeyRange {
    /**
     *  A value between 0 and 1, the proportion of keys equal to the
     *  specified key.
     */
    public double equal;
    /**
     *  A value between 0 and 1, the proportion of keys greater than
     *  the specified key.
     */
    public double greater;
    /**
     *  A value between 0 and 1, the proportion of keys less than the
     *  specified key.
     */
    public double less;
}
