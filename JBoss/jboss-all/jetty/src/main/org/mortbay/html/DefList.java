// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: DefList.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;
import java.io.IOException;
import java.io.Writer;
import java.util.Vector;

// =======================================================================
public class DefList extends Element
{

    // ------------------------------------------------------------
    public DefList()
    {
        terms = new Vector();
        defs = new Vector();
    }

    // ------------------------------------------------------------
    public void add(Element term, Element def)
    {
        terms.addElement(term);
        defs.addElement(def);
    }

    // ------------------------------------------------------------
    public void write(Writer out)
         throws IOException
    {
        out.write("<dl"+attributes()+">");

        if (terms.size() != defs.size())
            throw new Error("mismatched Vector sizes");

        for (int i=0; i <terms.size() ; i++)
        {
            out.write("<dt>");
            ((Element)terms.elementAt(i)).write(out);
            out.write("</dt><dd>");
            ((Element)defs.elementAt(i)).write(out);
            out.write("</dd>");
        }

        out.write("</dl>");
    }

    // ------------------------------------------------------------
    private Vector terms;
    private Vector defs;
}

