// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Observed.java,v 1.15.2.3 2003/06/04 04:47:58 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.util;

import java.util.Observable;

/* ======================================================================== */
/** Helpful extension to Observable.
 * NotifyObservers will set a changed first.
 */
public class Observed  extends Observable
{
    public void notifyObservers(Object arg)
    {
        setChanged();
        super.notifyObservers(arg);
    }

    public void notifyObservers()
    {
        setChanged();
        super.notifyObservers(null);
    }
}
