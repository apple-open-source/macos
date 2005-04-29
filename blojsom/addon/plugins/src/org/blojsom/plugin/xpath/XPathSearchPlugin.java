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
package org.blojsom.plugin.xpath;

import org.apache.commons.jxpath.JXPathContext;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.search.SimpleSearchPlugin;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * XPathSearchPlugin
 *
 * @author Mark Lussier
 * @since blojsom 2.02
 * @version $Id: XPathSearchPlugin.java,v 1.2 2004/08/27 00:29:23 whitmore Exp $
 */

public class XPathSearchPlugin extends SimpleSearchPlugin {

    private Log _logger = LogFactory.getLog(XPathSearchPlugin.class);

    /**
     * Process the blog entries
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link org.blojsom.blog.BlogUser} instance
     * @param context Context
     * @param entries Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {

        BlogEntry[] results = null;

        String query = httpServletRequest.getParameter(QUERY_PARAM);

        if (query != null) {
            query = query.trim();
            if (query.startsWith("/")) {
                _logger.debug("Attempting xpath query with: " + query);
                BlogEntryWrapper entryWrapper = new BlogEntryWrapper(entries);
                List foundEntries = new ArrayList();
                JXPathContext xpathcontext = JXPathContext.newContext(entryWrapper);
                try {
                    Iterator entryIterator = xpathcontext.iterate(query);
                    while (entryIterator.hasNext()) {
                        Object object = entryIterator.next();
                        BlogEntry entry = (BlogEntry) object;
                        foundEntries.add(entry);
                    }
                } catch (Exception e) {
                    _logger.error(e);
                }

                if (foundEntries.size() == 0) {
                    results = new BlogEntry[0];
                } else {
                    results = new BlogEntry[foundEntries.size()];
                    for (int x = 0; x < foundEntries.size(); x++) {
                        results[x] = (BlogEntry) foundEntries.get(x);
                    }
                }
            } else {
                results = super.process(httpServletRequest, httpServletResponse, user, context, entries);
            }
        } else {
            results = entries;
        }

        return results;
    }
}
