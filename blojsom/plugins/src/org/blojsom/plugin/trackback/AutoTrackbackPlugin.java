/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
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
package org.blojsom.plugin.trackback;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomConstants;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * AutoTrackbackPlugin
 *
 * @author David Czarnecki
 * @since blojsom 2.02
 * @version $Id: AutoTrackbackPlugin.java,v 1.2.2.1 2005/07/21 04:30:43 johnan Exp $
 */
public class AutoTrackbackPlugin implements BlojsomPlugin, BlojsomConstants {

    private Log _logger = LogFactory.getLog(AutoTrackbackPlugin.class);

    private static final int REGEX_OPTIONS = Pattern.DOTALL | Pattern.MULTILINE | Pattern.CASE_INSENSITIVE;
    private static final Pattern RDF_OUTER_PATTERN = Pattern.compile("(<rdf:RDF.*?</rdf:RDF>).*?", REGEX_OPTIONS);
    private static final Pattern RDF_INNER_PATTERN = Pattern.compile("(<rdf:Description.*/>)", REGEX_OPTIONS);
    private static final Pattern DC_IDENTIFIER_PATTERN = Pattern.compile("dc:identifier=\"(.*)\"");
    private static final Pattern TRACKBACK_PING_PATTERN = Pattern.compile("trackback:ping=\"(.*)\"");
    private static final Pattern HREF_PATTERN = Pattern.compile("<\\s*a.*href\\s*=\\s*\"(([^\"]+).*?)\"\\s*>", REGEX_OPTIONS);

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
    }


    /**
     * Perform the trackback autodiscovery process
     *
     * @param blog Blog information
     * @param blogEntry Blog entry
     */
    private void trackbackAutodiscovery(Blog blog, BlogEntry blogEntry) {
        try {
            // Build the URL parameters for the trackback ping URL
            StringBuffer trackbackPingURLParameters = new StringBuffer();
            trackbackPingURLParameters.append("&").append(TrackbackPlugin.TRACKBACK_URL_PARAM).append("=").append(blogEntry.getLink());
            trackbackPingURLParameters.append("&").append(TrackbackPlugin.TRACKBACK_TITLE_PARAM).append("=").append(URLEncoder.encode(blogEntry.getTitle(), UTF8));
            trackbackPingURLParameters.append("&").append(TrackbackPlugin.TRACKBACK_BLOG_NAME_PARAM).append("=").append(URLEncoder.encode(blog.getBlogName(), UTF8));

            String excerpt = blogEntry.getDescription().replaceAll("<.*?>", "");
            if (excerpt.length() > 255) {
                excerpt = excerpt.substring(0, 251);
                excerpt += "...";
            }
            trackbackPingURLParameters.append("&").append(TrackbackPlugin.TRACKBACK_EXCERPT_PARAM).append("=").append(URLEncoder.encode(excerpt, UTF8));
            
            // Extract all the HREF links from the blog description
            Matcher hrefMatcher = HREF_PATTERN.matcher(blogEntry.getDescription());
            while (hrefMatcher.find()) {

                // If we have a group count of 2, the inner group will be the http:// reference
                // Read the entire contents of the URL into a buffer
                if (hrefMatcher.groupCount() == 2) {
                    String hyperlink = hrefMatcher.group(1);
                    _logger.debug("Found hyperlink: " + hyperlink);
                    BufferedReader br;
                    URL hyperlinkURL = new URL(hyperlink);
                    br = new BufferedReader(new InputStreamReader(hyperlinkURL.openStream()));
                    String html;
                    StringBuffer contents = new StringBuffer();
                    while ((html = br.readLine()) != null) {
                        contents.append(html).append("\n");
                    }

                    // Look for the Auto Trackback RDF in the HTML
                    Matcher rdfOuterMatcher = RDF_OUTER_PATTERN.matcher(contents.toString());
                    while (rdfOuterMatcher.find()) {
                        _logger.debug("Found outer RDF text in hyperlink");
                        for (int i = 0; i < rdfOuterMatcher.groupCount(); i++) {
                            String outerRdfText = rdfOuterMatcher.group(i);

                            // Look for the inner RDF description
                            Matcher rdfInnerMatcher = RDF_INNER_PATTERN.matcher(outerRdfText);
                            while (rdfInnerMatcher.find()) {
                                _logger.debug("Found inner RDF text in hyperlink");
                                for (int j = 0; j < rdfInnerMatcher.groupCount(); j++) {
                                    String innerRdfText = rdfInnerMatcher.group(j);

                                    // Look for a dc:identifier attribute which matches the current hyperlink
                                    Matcher dcIdentifierMatcher = DC_IDENTIFIER_PATTERN.matcher(innerRdfText);
                                    if (dcIdentifierMatcher.find()) {
                                        String dcIdentifier = dcIdentifierMatcher.group(1);

                                        // If we find a match, send a trackback ping to the
                                        if (dcIdentifier.equals(hyperlink)) {
                                            _logger.debug("Matched dc:identifier to hyperlink");
                                            Matcher trackbackPingMatcher = TRACKBACK_PING_PATTERN.matcher(innerRdfText);
                                            if (trackbackPingMatcher.find()) {
                                                StringBuffer trackbackPingURL = new StringBuffer(trackbackPingMatcher.group(1));

                                                _logger.debug("Automatically sending trackback ping to URL: " + trackbackPingURL.toString());
                                                URL trackbackUrl = new URL(trackbackPingURL.toString());

                                                // Open a connection to the trackback URL and read its input
                                                HttpURLConnection trackbackUrlConnection = (HttpURLConnection) trackbackUrl.openConnection();
                                                trackbackUrlConnection.setRequestMethod("POST");
                                                trackbackUrlConnection.setRequestProperty("Content-Encoding", UTF8);
                                                trackbackUrlConnection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");
                                                trackbackUrlConnection.setRequestProperty("Content-Length", "" + trackbackPingURLParameters.length());
                                                trackbackUrlConnection.setDoOutput(true);
                                                trackbackUrlConnection.getOutputStream().write(trackbackPingURLParameters.toString().getBytes(UTF8));
                                                trackbackUrlConnection.connect();
                                                BufferedReader trackbackStatus = new BufferedReader(new InputStreamReader(trackbackUrlConnection.getInputStream()));
                                                String line;
                                                StringBuffer status = new StringBuffer();
                                                while ((line = trackbackStatus.readLine()) != null) {
                                                    status.append(line).append("\n");
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } catch (IOException e) {
            _logger.error(e);
        }
    }


    /**
     * Process the blog entries
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link org.blojsom.blog.BlogUser} instance
     * @param context Context
     * @param entries Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        for (int i = 0; i < entries.length; i++) {
            BlogEntry entry = entries[i];
            if (entry.getMetaData() != null) {
                Map entryMetaData = entry.getMetaData();
                if (entryMetaData.containsKey("auto-trackback") && !entryMetaData.containsKey("auto-trackback-complete")) {
                    trackbackAutodiscovery(user.getBlog(), entry);
                    entryMetaData.put("auto-trackback-complete", "true");
                    try {
                        entry.save(user);
                    } catch (BlojsomException e) {
                        _logger.error(e);
                    }
                }
            } else {
                _logger.debug("Skipping blog entry for autotrackback: " + entry.getPermalink());
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
