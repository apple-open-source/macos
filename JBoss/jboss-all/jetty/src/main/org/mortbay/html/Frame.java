// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Frame.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;
import java.io.IOException;
import java.io.Writer;

/** FrameSet.
 * @version $Id: Frame.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
 * @author Greg Wilkins
*/
public class Frame
{
    String src=null;
    String name=null;
    
    String scrolling="auto";
    String resize="";
    String border="";
    
    /* ------------------------------------------------------------ */
    /** Frame constructor.
     */
    Frame(){}
    
    /* ------------------------------------------------------------ */
    public Frame border(boolean threeD, int width, String color)
    {
        border=" frameborder=\""+(threeD?"yes":"no")+"\"";
        if (width>=0)
            border+=" border=\""+width+"\"";

        if (color!=null)
            border+=" BORDERCOLOR=\""+color+"\"";
        return this;
    }
    /* ------------------------------------------------------------ */
    public Frame name(String name,String src)
    {
        this.name=name;
        this.src=src;
        return this;
    }
    
    /* ------------------------------------------------------------ */
    public Frame src(String s)
    {
        src=s;
        return this;
    }
    
    /* ------------------------------------------------------------ */
    public Frame name(String n)
    {
        name=n;
        return this;
    }

    /* ------------------------------------------------------------ */
    public Frame scrolling(boolean s)
    {
        scrolling=s?"yes":"no";
        return this;
    }
    
    /* ------------------------------------------------------------ */
    public Frame resize(boolean r)
    {
        resize=r?"":" noresize";
        return this;
    }
    
    /* ----------------------------------------------------------------- */
    void write(Writer out)
         throws IOException
    {
        out.write("<frame scrolling=\""+scrolling+"\""+resize+border);
        
        if(src!=null)
            out.write(" src=\""+src+"\"");
        if(name!=null)
            out.write(" name=\""+name+"\"");
        out.write(">");
    }
};






