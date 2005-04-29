/**
 * Copyright (c) 2003-2004, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004 by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.plugin.highlight;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The GoogleHighlightPlugin will highlight words on your blog if the referer came from a Google
 * query.
 * <p/>
 * Based on work from http://www.textism.com/
 *
 * @author Mark Lussier
 * @version $Id: GoogleHighlightPlugin.java,v 1.2 2004/08/27 01:06:38 whitmore Exp $
 */
public class GoogleHighlightPlugin implements BlojsomPlugin {

    /**
     * HTTP Header for Referer Information
     */
    private static final String HEADER_REFERER = "referer";

    private static final String START_BOUNDRY = "(\\b";
    private static final String END_BOUNDRY = "\\b)";

    /**
     * Expression used to identify the referer as a Google referer
     */
    private static final String EXPRESSSION_GOOGLE = "^http:\\/\\/w?w?w?\\.?google.*";

    private static final String EXPRESSION_HTMLPREFIX = "(?<=>)([^<]+)?";
    private static final String EXPRESSION_HASTAGS = "<.+>";

    /**
     * Expression used to extract the Query string portion of the referer
     */
    private static final String GOOGLE_QUERY = "^.*q=([^&]+)&?.*$";

    /**
     * Expression used to clean quotes
     */
    private static final String GOOGLE_CLEANQUOTES = "'/\'|\"/\"";

    /**
     * Used to replace matches in entries that DO NOT have html tags
     */
    private static final String HIGHLIGHT_PLAINTEXT = "<span class=\"searchhighlight\">$1</span>";

    /**
     * Used to replace matches in entries that HAVE html tags
     */
    private static final String HIGHLIGHT_HTML = "$1<span class=\"searchhighlight\">$2</span>";


    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
    }


    /**
     * Extract search tokens from the Google Query String
     *
     * @param referer The Google referer
     * @return A string array of search words or <code>null</code> if no search query match is found
     */
    private String[] extractQueryTokens(String referer) {
        String[] result = null;
        Matcher matcher = Pattern.compile(GOOGLE_QUERY, Pattern.CASE_INSENSITIVE).matcher(referer);
        if (matcher.find()) {
            String _query = matcher.group(1);
            _query = _query.replaceAll(GOOGLE_CLEANQUOTES, "");
            StringTokenizer _st = new StringTokenizer(_query, "+, .", false);
            result = new String[_st.countTokens()];
            int cnt = 0;
            while (_st.hasMoreElements()) {
                result[cnt] = _st.nextToken();
                cnt += 1;
            }
        }

        return result;
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {
        String referer = httpServletRequest.getHeader(HEADER_REFERER);

        if (referer != null && referer.matches(EXPRESSSION_GOOGLE)) {
            String[] searchwords = extractQueryTokens(referer);

            if (searchwords != null) {
                Pattern hasTags = Pattern.compile(EXPRESSION_HASTAGS);

                for (int x = 0; x < entries.length; x++) {
                    BlogEntry entry = entries[x];
                    Matcher matcher = hasTags.matcher(entry.getDescription());
                    boolean isHtml = matcher.find();
                    for (int y = 0; y < searchwords.length; y++) {
                        String word = searchwords[y];
                        if (!isHtml) {
                            entry.setDescription(entry.getDescription().replaceAll(START_BOUNDRY + word + END_BOUNDRY,
                                    HIGHLIGHT_PLAINTEXT));
                        } else {
                            entry.setDescription(entry.getDescription().replaceAll(EXPRESSION_HTMLPREFIX + START_BOUNDRY
                                    + word + END_BOUNDRY,
                                    HIGHLIGHT_HTML));
                        }

                    }
                }
            }
        }

        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws BlojsomPluginException If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws BlojsomPluginException If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}
