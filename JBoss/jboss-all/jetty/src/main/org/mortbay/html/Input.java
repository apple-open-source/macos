// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Input.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;

/* -------------------------------------------------------------------- */
/** HTML Form Input Tag.
 * <p>
 * @see Tag
 * @see Form
 * @version $Id: Input.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
 * @author Greg Wilkins
 */
public class Input extends Tag
{
    /* ----------------------------------------------------------------- */
    /** Input types */
    public final static String Text="text";
    public final static String Password="password";
    public final static String Checkbox="checkbox";
    public final static String Radio="radio";
    public final static String Submit="submit";
    public final static String Reset="reset";
    public final static String Hidden="hidden";
    public final static String File="file";
    public final static String Image="image";

    /* ----------------------------------------------------------------- */
    public Input(String type,String name)
    {
        super("input");
        attribute("type",type);
        attribute("name",name);
    }

    /* ----------------------------------------------------------------- */
    public Input(String type,String name, String value)
    {
        this(type,name);
        attribute("value",value);
    }

    /* ----------------------------------------------------------------- */
    public Input(Image image,String name, String value)
    {
        super("input");
        attribute("type","image");
        attribute("name",name);
        if (value!=null)
            attribute("value",value);
        attribute(image.attributes());
    }
    
    /* ----------------------------------------------------------------- */
    public Input(Image image,String name)
    {
        super("input");
        attribute("type","image");
        attribute("name",name);
        attribute(image.attributes());
    }

    /* ----------------------------------------------------------------- */
    public Input check()
    {
        attribute("checked");
        return this;
    }

    /* ----------------------------------------------------------------- */
    public Input setSize(int size)
    {
        size(size);
        return this;
    }

    /* ----------------------------------------------------------------- */
    public Input setMaxSize(int size)
    {
        attribute("maxlength",size);
        return this;
    }

    /* ----------------------------------------------------------------- */
    public Input fixed()
    {
        setMaxSize(size());
        return this;
    }
}
