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
package org.blojsom.plugin.emoticons;

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
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;
import java.util.Properties;

/**
 * EmoticonsPlugin
 *
 * @author David Czarnecki
 * @version $Id: EmoticonsPlugin.java,v 1.2.2.1 2005/07/21 04:30:30 johnan Exp $
 */
public class EmoticonsPlugin implements BlojsomPlugin {

    private static final String EMOTICONS_CONFIGURATION_IP = "plugin-emoticons";
    private static final String BLOJSOM_PLUGIN_METADATA_EMOTICONS_DISABLED = "emoticons-disabled";

    private static final String HAPPY = ":)";
    private static final String HAPPY_PARAM = "happy";

    private static final String SAD = ":(";
    private static final String SAD_PARAM = "sad";

    private static final String GRIN = ":D";
    private static final String GRIN_PARAM = "grin";

    private static final String LOVE = "<3";
    private static final String LOVE_PARAM = "love";

    private static final String MISCHIEF = ";7)";
    private static final String MISCHIEF_PARAM = "mischief";

    private static final String COOL = "])";
    private static final String COOL_PARAM = "cool";

    private static final String DEVIL = "})";
    private static final String DEVIL_PARAM = "devil";

    private static final String SILLY = ":P";
    private static final String SILLY_PARAM = "silly";

    private static final String ANGRY = ">(";
    private static final String ANGRY_PARAM = "angry";

    private static final String LAUGH = "(D";
    private static final String LAUGH_PARAM = "laugh";

    private static final String WINK = ";)";
    private static final String WINK_PARAM = "wink";

    private static final String BLUSH = "*^_^*";
    private static final String BLUSH_PARAM = "blush";

    private static final String CRY = ":'(";
    private static final String CRY_PARAM = "cry";

    private static final String CONFUSED = "`:|";
    private static final String CONFUSED_PARAM = "confused";

    private static final String SHOCKED = ":O";
    private static final String SHOCKED_PARAM = "shocked";

    private static final String PLAIN = ":|";
    private static final String PLAIN_PARAM = "plain";

    private static final String IMG_OPEN = "<img src=\"";
    private static final String IMG_CLOSE = "\"";
    private static final String IMG_ALT_START = " alt=\"";
    private static final String IMG_ALT_END = "\" />";
    private static final String EMOTICONS_CLASS = " class=\"emoticons\" ";

    private Log _logger = LogFactory.getLog(EmoticonsPlugin.class);

    private ServletConfig _servletConfig;
    private BlojsomConfiguration _blojsomConfiguration;
    private String _emoticonsConfigurationFile;

    /**
     * Default constructor
     */
    public EmoticonsPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws org.blojsom.plugin.BlojsomPluginException {
        _servletConfig = servletConfig;
        _blojsomConfiguration = blojsomConfiguration;

        _emoticonsConfigurationFile = servletConfig.getInitParameter(EMOTICONS_CONFIGURATION_IP);
        if (BlojsomUtils.checkNullOrBlank(_emoticonsConfigurationFile)) {
            throw new BlojsomPluginException("No value given for: " + EMOTICONS_CONFIGURATION_IP + " configuration parameter");
        }
    }

    /**
     * Read in a properties file containing the mapping between the emoticon names and image files and return
     * the mapping as a {@link Map}
     *
     * @param blogID Blog ID
     * @return {@link Map} between emoticon names and image files or <code>null</code> if the mapping file could not
     *         be read
     */
    private Map readEmoticonsMapForBlog(String blogID) {
        Map emoticonsMap = null;

        Properties emoticonsProperties = new Properties();
        String configurationFile = _blojsomConfiguration.getBaseConfigurationDirectory() + blogID + '/' + _emoticonsConfigurationFile;
        InputStream is = _servletConfig.getServletContext().getResourceAsStream(configurationFile);
        if (is == null) {
            _logger.info("No emoticons configuration file found: " + configurationFile);
        } else {
            try {
                emoticonsProperties.load(is);
                is.close();

                return BlojsomUtils.propertiesToMap(emoticonsProperties);
            } catch (IOException e) {
                _logger.error(e);
            }
        }

        return emoticonsMap;
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error processing the blog entries
     */
    public org.blojsom.blog.BlogEntry[] process(HttpServletRequest httpServletRequest,
                                                HttpServletResponse httpServletResponse,
                                                BlogUser user,
                                                Map context,
                                                org.blojsom.blog.BlogEntry[] entries) throws org.blojsom.plugin.BlojsomPluginException {
        Map emoticonsForBlog = readEmoticonsMapForBlog(user.getId());
        if (emoticonsForBlog == null) {
            return entries;
        }

        String blogBaseUrl = user.getBlog().getBlogBaseURL();
        for (int i = 0; i < entries.length; i++) {
            BlogEntry entry = entries[i];
            if (!BlojsomUtils.checkMapForKey(entry.getMetaData(), BLOJSOM_PLUGIN_METADATA_EMOTICONS_DISABLED)) {
                String updatedDescription = entry.getDescription();
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, HAPPY, HAPPY_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, SAD, SAD_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, GRIN, GRIN_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, LOVE, LOVE_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, MISCHIEF, MISCHIEF_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, COOL, COOL_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, DEVIL, DEVIL_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, SILLY, SILLY_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, ANGRY, ANGRY_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, LAUGH, LAUGH_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, WINK, WINK_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, BLUSH, BLUSH_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, CRY, CRY_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, CONFUSED, CONFUSED_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, SHOCKED, SHOCKED_PARAM, blogBaseUrl);
                updatedDescription = replaceEmoticon(emoticonsForBlog, updatedDescription, PLAIN, PLAIN_PARAM, blogBaseUrl);
                entry.setDescription(updatedDescription);
            }
        }
        return entries;
    }

    /**
     * Replace the references in the description with the URL to the image for the emoticon
     *
     * @param emoticonString Description string
     * @param emoticon       Emoticon characters
     * @param emoticonParam  Emoticon property name
     * @param url            Base URL for the blog
     * @return Updated description with emoticon references replaced with URLs to the images for the emoticons
     */
    private String replaceEmoticon(Map emoticonsForUser, String emoticonString, String emoticon, String emoticonParam, String url) {
        String emoticonImage;
        emoticonImage = (String) emoticonsForUser.get(emoticonParam);
        if (emoticonImage != null && !"".equals(emoticonImage)) {
            StringBuffer imageReference = new StringBuffer(IMG_OPEN);
            imageReference.append(url);
            imageReference.append(emoticonImage);
            imageReference.append(IMG_CLOSE);
            imageReference.append(EMOTICONS_CLASS);
            imageReference.append(IMG_ALT_START);
            imageReference.append(emoticonImage);
            imageReference.append(IMG_ALT_END);
            return BlojsomUtils.replace(emoticonString, emoticon, imageReference.toString());
        }

        return emoticonString;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}
