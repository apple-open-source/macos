// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Style.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
// ========================================================================

package org.mortbay.html;


/* -------------------------------------------------------------------- */
/** HTML Style Block.
 */
public class Style extends Block
{
    public static final String
        STYLE = "style",
        TYPE  = "type",
        MEDIA = "media";
    
    public final static String
        StyleSheet="stylesheet",
        AlternateStyleSheet="alternate stylesheet",
        text_css="text/css",
        screen = "screen";

    
    /* ------------------------------------------------------------ */
    /** Construct a Style element.
     * @param type Format of Style */
    public Style(String style, String type)
    {
        super(STYLE);
        if (type!=null)
            attribute(TYPE,type);
        add(style);
    }

    /* ------------------------------------------------------------ */
    /** Construct a Style element */
    public Style(String style)
    {
        this(style, text_css);
    }
    
    /* ------------------------------------------------------------ */
    /** Construct a Style element */
    public Style()
    {
        super(STYLE);
        attribute(TYPE,text_css);
    }
    
    /* ------------------------------------------------------------ */
    /** Set the media
     */
    public Style media(String m)
    {
        attribute(MEDIA,m);
        return this;
    }
    
    /* ------------------------------------------------------------ */
    /** Nest style content in comment 
     */
    public Style comment()
    {
        nest(new Comment());
        return this;
    }


    /* ------------------------------------------------------------ */
    /** Import another style sheet.
     * @param url The URL to import
     * @return This style
     */
    public Style importStyle(String url)
    {
        add("@import url("+url+");\n");
        return this;
    }
};




