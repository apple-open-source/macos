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

import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Transport;
import javax.mail.internet.AddressException;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMessage;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Date;


/**
 * Send Email (SMTP) Plugin Support Utilities. This class is used to put email into the plugin context for the email
 * plugin to get at
 *
 * @author Mark Lussier
 * @version $Id: EmailUtils.java,v 1.2 2004/08/27 01:06:37 whitmore Exp $
 */
public class EmailUtils {

    /**
     * Logger Instance
     */
    private static Log _logger = LogFactory.getLog(EmailUtils.class);

    /**
     * Variable name for the message arraylist in the context on the plugin chain
     */
    public static final String BLOJSOM_OUTBOUNDMAIL = "BLOJSOM_OUTBOUNDMAIL";


    /**
     * Ensure that the ArrayList exists in the context, otherwise create it
     *
     * @param context Context
     */
    private static void checkContext(Map context) {
        if (!context.containsKey(BLOJSOM_OUTBOUNDMAIL)) {
            context.put(BLOJSOM_OUTBOUNDMAIL, new ArrayList(1));
        }
    }


    /**
     * Send notification email to the Blog Author. This utilizes two values in blojsom.properties to determine the name
     * and email address to use
     *
     * @param subject Subject of the message being sent
     * @param message The message text
     * @param context The context Map for putting the messages
     */
    public static void notifyBlogAuthor(String subject, String message, Map context) {
        checkContext(context);
        List _messagelist = (List) context.get(BLOJSOM_OUTBOUNDMAIL);
        _messagelist.add(new EmailMessage(subject, message));
        context.put(BLOJSOM_OUTBOUNDMAIL, _messagelist);
    }


    /**
     * Helper method to create the recipient email address for a giving email message. This will default to the blog
     * author is no recipient email is provided.
     *
     * @param recipient Email address of message recipient
     * @param defaultname Default recipient name
     * @param defaultemail Default recipient email
     * @return properly formatted InternetAddress instance
     * @throws UnsupportedEncodingException
     * @throws AddressException
     */
    public static InternetAddress constructRecipientAddress(String recipient, String defaultname, String defaultemail)
            throws UnsupportedEncodingException, AddressException {

        InternetAddress result = null;
        if (recipient == null) {
            result = new InternetAddress(defaultemail, defaultname);

        } else {
            result = new InternetAddress(recipient);
        }

        return result;
    }

    /**
     * Helper method to create the sender email address for a giving email message. This will default to the blog
     * author is no recipient email is provided.
     *
     * @param sender Email address of message sender
     * @param defaultname Default sender name
     * @param defaultemail Default email name
     * @return properly formatted InternetAddress instance
     * @throws UnsupportedEncodingException
     * @throws AddressException
     */
    public static InternetAddress constructSenderAddress(String sender, String defaultname, String defaultemail)
            throws UnsupportedEncodingException, AddressException {

        InternetAddress result = null;
        if (sender == null) {
            result = new InternetAddress(defaultemail, defaultname);

        } else {
            result = new InternetAddress(sender);
        }

        return result;
    }

    /**
     * Send an Email Message
     * @param mailsession Session Inastance
     * @param emailmessage EmailMessage Instance
     * @param defaultaddress InternetAddress Instance of Recipient/Sender
     */
    public static void sendMailMessage(Session mailsession, EmailMessage emailmessage, InternetAddress defaultaddress) {
        try {
            MimeMessage message = new MimeMessage(mailsession);
            InternetAddress _msgto;
            InternetAddress _msgfrom;

            /* Create the From Address */
            _msgfrom = constructSenderAddress(emailmessage.getFrom(), "blojsom", defaultaddress.getAddress());

            /* Create the To Address */
            _msgto = defaultaddress;

            message.setFrom(_msgfrom);
            message.addRecipient(Message.RecipientType.TO, _msgto);
            message.setSubject(emailmessage.getSubject());
            message.setText(emailmessage.getMessage());
            message.setSentDate(new Date());

            _logger.info("Sending e-mail message to: " + _msgto.getAddress());

            /* Send the email. BLOCKING CALL!! */
            Transport.send(message);

        } catch (UnsupportedEncodingException e) {
            _logger.error(e);
        } catch (MessagingException e) {
            _logger.error(e);
        }

    }

}
