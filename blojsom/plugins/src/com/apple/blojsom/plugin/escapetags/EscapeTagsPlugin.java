/**
 * Contains:   Inline administration plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: EscapeTagsPlugin.java,v 1.3 2004/11/18 18:31:06 johnan Exp $
 */ 
package com.apple.blojsom.plugin.escapetags;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;

/**
 * Escape Tags plug-in
 *
 * @author John Anderson
 * @version $Id: EscapeTagsPlugin.java,v 1.3 2004/11/18 18:31:06 johnan Exp $
 */

public class EscapeTagsPlugin implements BlojsomPlugin {

	// constants
	private static final String[] ALLOWED_TAGS = {
		"a", "b", "blockquote", "br", "code", "dd", "dl", "div", "em", "font", "h1",
		"h2", "h3", "h4", "h5", "h6", "i", "img", "ol", "li", "p", "pre", "span", "strong",
		"sub", "sup", "table", "td", "th", "tr", "u", "ul"
	};
	
	protected static final String ESCAPE_TAGS_PLUGIN = "ESCAPE_TAGS_PLUGIN";
	protected static final String VALID_TAG_SEARCH = "<(/*)([Aa]|[Bb]|[Bb][Ll][Oo][Cc][Kk][Qq][Uu][Oo][Tt][Ee]|[Bb][Rr]|[Cc][Oo][Dd][Ee]|[Dd][Dd]|[Dd][Ll]|[Dd][Ii][Vv]|[Ee][Mm]|[Ff][Oo][Nn][Tt]|[Hh]1|[Hh]2|[Hh]3|[Hh]4|[Hh]5|[Hh]6|[Ii]|[Ii][Mm][Gg]|[Oo][Ll]|[Ll][Ii]|[Pp]|[Pp][Rr][Ee]|[Ss][Pp][Aa][Nn]|[Ss][Tt][Rr][Oo][Nn][Gg]|[Ss][Uu][Bb]|[Ss][Uu][Pp]|[Tt][Aa][Bb][Ll][Ee]|[Tt][Dd]|[Tt][Hh]|[Tt][Rr]|[Uu]|[Uu][Ll])( [^>]+>|>)";
	protected static final String VALID_TAG_REPLACE = "«$1$2$3»";
	protected static final String EXTRA_BRACKET_SEARCH = ">»";
	protected static final String EXTRA_BRACKET_REPLACE = "»";
	protected static final String REMAINING_LEFTBRACKET_SEARCH = "<";
	protected static final String REMAINING_LEFTBRACKET_REPLACE = "&lt;";
	protected static final String REMAINING_RIGHTBRACKET_SEARCH = ">";
	protected static final String REMAINING_RIGHTBRACKET_REPLACE = "&gt;";
	protected static final String LEFT_CHEVRON_SEARCH = "«";
	protected static final String LEFT_CHEVRON_REPLACE = "<";
	protected static final String RIGHT_CHEVRON_SEARCH = "»";
	protected static final String RIGHT_CHEVRON_REPLACE = ">";
	
    /**
     * Default constructor.
     */
    public EscapeTagsPlugin() {
    }
	
    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
    }
    
     /**
     * Escape tags in text
     *
     * @param textToEscape The text to escape HTML in.
     */
   public String escapeTagsInText(String textToEscape) {
		String escapedText = textToEscape;
	
		// first, replace the valid tags with «tag>»
		escapedText = escapedText.replaceAll(VALID_TAG_SEARCH, VALID_TAG_REPLACE);
		
		// now replace «tag>» with «tag» (easier to do it afterward)
		escapedText = escapedText.replaceAll(EXTRA_BRACKET_SEARCH, EXTRA_BRACKET_REPLACE);
		
		// now eradicate any remaining < and > symbols...
		escapedText = escapedText.replaceAll(REMAINING_LEFTBRACKET_SEARCH, REMAINING_LEFTBRACKET_REPLACE);
		escapedText = escapedText.replaceAll(REMAINING_RIGHTBRACKET_SEARCH, REMAINING_RIGHTBRACKET_REPLACE);
		
		// and turn the tags back to normal brackets
		escapedText = escapedText.replaceAll(LEFT_CHEVRON_SEARCH, LEFT_CHEVRON_REPLACE);
		escapedText = escapedText.replaceAll(RIGHT_CHEVRON_SEARCH, RIGHT_CHEVRON_REPLACE);
		
		return escapedText;
   }
   
    /**
     * Process the blog entries
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link BlogUser} instance
     * @param context Context
     * @param entries Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
		String currentEntryDescription;
		String currentEntryTitle;
		
		// add ourselves to the context (for additional escaping)
		context.put(ESCAPE_TAGS_PLUGIN, this);
		
        for (int i = 0; i < entries.length; i++) {
            BlogEntry entry = entries[i];
            
            // update the title
            currentEntryTitle = escapeTagsInText(entry.getTitle());
            entry.setTitle(currentEntryTitle);
            
            // update the description
            currentEntryDescription = escapeTagsInText(entry.getDescription());
            entry.setDescription(currentEntryDescription);
        }
		
		return entries;
	}
	
    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}
