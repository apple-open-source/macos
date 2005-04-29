/**
 * Contains:   Inline administration plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: LocalizerPlugin.java,v 1.7 2005/02/17 00:10:15 johnan Exp $
 */ 
package com.apple.blojsom.plugin.localizer;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.util.Hashtable;
import java.util.Properties;
import java.util.Date;
import java.util.Locale;
import java.text.DateFormat;

/**
 * Convert Line Breaks plug-in
 *
 * @author John Anderson
 * @version $Id: LocalizerPlugin.java,v 1.7 2005/02/17 00:10:15 johnan Exp $
 */

public class LocalizerPlugin implements BlojsomPlugin {

	// constants
	private static final String LINEBREAK_REGEX = "[\\r\\n]";
	private static final String HTMLBREAK_REPLACEMENT_REGEX = "&nbsp;<br />";
	protected static final Log _logger = LogFactory.getLog(LocalizerPlugin.class);
	
	protected Hashtable _stringsHash = new Hashtable();
	protected ServletConfig _servletConfig;

    /**
     * Default constructor.
     */
    public LocalizerPlugin() {
    }
	
    /**
     * Load all the strings files.
     */
	public void loadStringsFiles() {
		// get a listing of all of the stuff in the directory
		Object[] stringsFilenames = _servletConfig.getServletContext().getResourcePaths("/strings/").toArray();
		String currentStringsFilename;
		Properties currentStrings;
		
		for (int i = 0; i < stringsFilenames.length; i++) {
			currentStringsFilename = (String)stringsFilenames[i];
			try {
				currentStrings = BlojsomUtils.loadProperties(_servletConfig, currentStringsFilename);
				currentStringsFilename = currentStringsFilename.replaceAll("/strings/(.+)\\.properties", "$1");
				_stringsHash.put(currentStringsFilename, currentStrings);
				_logger.debug("adding loc properties for language: " + currentStringsFilename);
			} catch (org.blojsom.BlojsomException e) {
				// don't store the hash if we couldn't read the file
			}
		}
    }

    /**
     * Format a date and time according to the client browser's locale.
	 *
	 * @param dateToLocalize The date you wish to localize.
	 * @param localeString The two-letter locale.
	 * @param useExtendedFormat true to use Date.LONG, false for Date.SHORT
	 * @return The localized date/time string.
     */
	public String localizeDate(Date dateToLocalize, String localeString, boolean useExtendedFormat) {
		int formatType = DateFormat.SHORT;
		if (useExtendedFormat) {
			formatType = DateFormat.LONG;
		}
		Locale locale = new Locale(localeString);
		return DateFormat.getDateTimeInstance(formatType, formatType, locale).format(dateToLocalize);
	}

    /**
     * Format a date and time according to the client browser's locale.
	 *
	 * @param dateToLocalize The date you wish to localize.
	 * @param localeString The two-letter locale.
	 * @return The localized date/time string.
     */
    	public String localizeDate(Date dateToLocalize, String localeString) {
    		return localizeDate(dateToLocalize, localeString, true);
    	}
    	
    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
		_servletConfig = servletConfig;
		loadStringsFiles();
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
        String language = httpServletRequest.getHeader("accept-language");
		
		// comment this out when in non-dev mode
		loadStringsFiles();
		
		if (language == null) {
			language = "en";
		}
		else {
			language = language.replaceAll("^([a-z]+).*$", "$1");
		}
				
		context.put("acceptLanguageHeader", language);
		
		Properties locStrings = (Properties)_stringsHash.get(language);
		
		// if we couldn't find the language, give them English
		if (locStrings == null) {
			language = "en";
			locStrings = (Properties)_stringsHash.get("en");
		}
		
		// if all else fails (although it shouldn't), give them an empty strings list
		if (locStrings == null) {
			locStrings = new Properties();
		}
		
		// so that $locStrings.getProperty("stringName") will return a localized string
		context.put("locStrings", locStrings);
		context.put("locLanguage", language);
		context.put("locPlugin", this);
				
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
