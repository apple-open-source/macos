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
package org.blojsom.plugin.email;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.mail.Session;
import javax.mail.internet.InternetAddress;
import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.UnsupportedEncodingException;
import java.util.List;
import java.util.Map;
import java.util.Properties;

/**
 * Send Email (SMTP) Plugin
 *
 * @author Mark Lussier
 * @version $Id: SendEmailPlugin.java,v 1.2 2004/08/27 01:06:37 whitmore Exp $
 */
public class SendEmailPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(SendEmailPlugin.class);

    /**
     * SMTP server initialization parameter
     */
    public static final String SMTPSERVER_IP = "smtp-server";

    /**
     * SMTP session name
     */
    public static final String SESSION_NAME = "mail.smtp.host";


    /**
     * JavaMail Session Object
     */
    private Session _mailsession = null;

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        String _hostname = servletConfig.getInitParameter(SMTPSERVER_IP);
        if (_hostname != null) {
            Properties _props = new Properties();
            _props.put(SESSION_NAME, _hostname);
            _mailsession = Session.getInstance(_props);
        }

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
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {
        Blog blog = user.getBlog();

        String _defaultrecipientname = null;
        String _defaultrecipientemail = null;

        if (blog.getBlogOwnerEmail() != null) {
            _defaultrecipientemail = blog.getBlogOwnerEmail();
        }

        if (blog.getBlogOwner() != null) {
            _defaultrecipientname = blog.getBlogOwner();
        }
        List _messagelist = (List) context.get(EmailUtils.BLOJSOM_OUTBOUNDMAIL);
        if (_messagelist != null) {
            for (int x = 0; x < +_messagelist.size(); x++) {
                EmailMessage _msg = (EmailMessage) _messagelist.get(x);
                sendMailMessage(_msg, _defaultrecipientemail, _defaultrecipientname);

            }
        }

        return entries;
    }


    /**
     * Send the email message
     *
     * @param emailmessage Email message
     */
    private void sendMailMessage(EmailMessage emailmessage, String defaultRecipientEmail, String defaultRecipientName) {
        try {
            InternetAddress defaultAddress = new InternetAddress(defaultRecipientEmail, defaultRecipientName);
            EmailUtils.sendMailMessage(_mailsession, emailmessage, defaultAddress);
        } catch (UnsupportedEncodingException e) {
            _logger.error(e);
        }
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
