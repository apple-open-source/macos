/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: PartKey.java,v 1.2 2004/03/30 01:23:24 jtownsen Exp $
 */

package com.sleepycat.examples.bdb.shipment.sentity;

/**
 * A PartKey serves as the key in the key/value pair for a part entity.
 *
 * <p> In this sample, PartKey is bound to the key's tuple storage data using
 * a TupleBinding.  Because it is not used directly as storage data, it does
 * not need to be Serializable. </p>
 *
 * @author Mark Hayes
 */
public class PartKey {

    private String number;

    public PartKey(String number) {

        this.number = number;
    }

    public final String getNumber() {

        return number;
    }

    public String toString() {

        return "[PartKey: number=" + number + ']';
    }
}
