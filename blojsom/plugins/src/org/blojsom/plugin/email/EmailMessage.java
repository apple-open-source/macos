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
package org.blojsom.plugin.email;


/**
 * Send Email (SMTP) Plugin Message. This exists since when EmailUtils creates the messages it wont have access to the
 * Session instance held by SendEmailPlugin.
 *
 * @author Mark Lussier
 * @version $Id: EmailMessage.java,v 1.2.2.1 2005/07/21 04:30:30 johnan Exp $
 */
public class EmailMessage {

    String _from;
    String _to;
    String _subject;
    String _message;

    /**
     * Email message constructor
     * 
     * @param subject E-mail subject
     * @param message E-mail message
     */
    public EmailMessage(String subject, String message) {
        _from = null;
        _to = null;
        _subject = subject;
        _message = message;
    }

    /**
     * E-mail message constructor
     *
     * @param from From
     * @param to To
     * @param subject E-mail subject
     * @param message E-mail message
     */
    public EmailMessage(String from, String to, String subject, String message) {
        _from = from;
        _to = to;
        _subject = subject;
        _message = message;
    }

    /**
     * Get the from: address for the e-mail
     *
     * @return From: address
     */
    public String getFrom() {
        return _from;
    }

    /**
     * Set the from: address for the e-mail
     *
     * @param from From: address
     */
    public void setFrom(String from) {
        _from = from;
    }

    /**
     * Get the to: address for the e-mail
     *
     * @return To: address
     */
    public String getTo() {
        return _to;
    }

    /**
     * Set the to: address for the e-mail
     *
     * @param to To: address
     */
    public void setTo(String to) {
        _to = to;
    }

    /**
     * Get the subject of the e-mail
     *
     * @return E-mail subject
     */
    public String getSubject() {
        return _subject;
    }

    /**
     * Set the subject of the e-mail
     *
     * @param subject E-mail subject
     */
    public void setSubject(String subject) {
        _subject = subject;
    }

    /**
     * Get the e-mail message content
     *
     * @return Message content
     */
    public String getMessage() {
        return _message;
    }

    /**
     * Set the message content for the e-mail
     *
     * @param message Message content
     */
    public void setMessage(String message) {
        _message = message;
    }
}
