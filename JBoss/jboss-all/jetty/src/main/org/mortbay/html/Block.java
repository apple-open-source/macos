// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Block.java,v 1.15.2.3 2003/06/04 04:47:36 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;
import java.io.IOException;
import java.io.Writer;

/* -------------------------------------------------------------------- */
/** HTML Block Composite.
 * Block of predefined or arbitrary type.
 * Block types are predefined for PRE, BLOCKQUOTE, CENTER, LISTING,
 * PLAINTEXT, XMP, DIV (Left and Right) and SPAN.
 * @see  org.mortbay.html.Composite
 */
public class Block extends Composite
{
    /* ----------------------------------------------------------------- */
    /** Preformatted text */
    public static final String Pre="pre";
    /** Quoted Text */
    public static final String Quote="blockquote";
    /** Center the block */
    public static final String Center="center";
    /** Code listing style */
    public static final String Listing="listing";
    /** Plain text */
    public static final String Plain="plaintext";
    /** Old pre format - preserve line breaks */
    public static final String Xmp="xmp";
    /** Basic Division */
    public static final String Div="div";
    /** Left align */
    public static final String Left="divl";
    /** Right align */
    public static final String Right="divr";
    /** Bold */
    public static final String Bold="b";
    /** Italic */
    public static final String Italic="i";
    /** Span */
    public static final String Span="span";

    /* ----------------------------------------------------------------- */
    private String tag;

    /* ----------------------------------------------------------------- */
    /** Construct a block using the passed string as the tag.
     * @param tag The tag to use to open and close the block.
     */
    public Block(String tag)
    {
        this.tag=tag;
        if (tag==Left)
        {
            tag=Div;
            left();
        }
        if (tag==Right)
        {
            tag=Div;
            right();
        }
    }

    /* ----------------------------------------------------------------- */
    /** Construct a block using the passed string as the tag.
     * @param tag The tag to use to open and close the block.
     * @param attributes String of attributes for opening tag.
     */
    public Block(String tag, String attributes)
    {
        super(attributes);
        this.tag=tag;
    }
        
    /* ----------------------------------------------------------------- */
    public void write(Writer out)
         throws IOException
    {
        out.write('<'+tag+attributes()+'>');
        super.write(out);
        out.write("</"+tag+"\n>");
    }
}


