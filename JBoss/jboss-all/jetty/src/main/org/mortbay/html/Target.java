// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Target.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;


/* -------------------------------------------------------------------- */
/** HTML Link Target.
 * This is a HTML reference (not a CSS Link).
 * @see StyleLink
 */
public class Target extends Block
{

    /* ----------------------------------------------------------------- */
    /** Construct Link.
     * @param target The target name 
     */
    public Target(String target)
    {
        super("a");
        attribute("name",target);
    }

    /* ----------------------------------------------------------------- */
    /** Construct Link.
     * @param target The target name 
     * @param link Link Element
     */
    public Target(String target,Object link)
    {
        this(target);
        add(link);
    }
}
