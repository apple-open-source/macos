// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Break.java,v 1.15.2.3 2003/06/04 04:47:36 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;

/* -------------------------------------------------------------------- */
/** Break Tag.
 */
public class Break extends Tag
{
    /* ---------------------------------------------------------------- */
    /** Line Break tag type */
    public final static String Line="br";
    /** Rule Break tag type */
    public final static String Rule="hr";
    /** Paragraph Break tag type */
    public final static String Para="p";

    /* ---------------------------------------------------------------- */
    /** Default constructor (Line Break).
     */
    public Break()
    {
        this(Line);
    }
    
    /* ---------------------------------------------------------------- */
    /** Constructor.
     * @param type The Break type
     */
    public Break(String type)
    {
        super(type);
    }
    
    /* ---------------------------------------------------------------- */
    /** Static instance of line break */
    public final static Break line=new Break(Line);
    /** Static instance of rule break */
    public final static Break rule=new Break(Rule);
    /** Static instance of paragraph break */
    public final static Break para=new Break(Para);

}

