// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: CompositeFactory.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;

/* --------------------------------------------------------------------- */
/** Composite Factory.
 * Abstract interface for production of composites
 */
public interface CompositeFactory
{
    public Composite newComposite();
}


