/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

// 3rd Party Packages
import javax.mail.Message;
import javax.mail.Session;
import javax.mail.Transport;

import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMessage;

// Standard Java Packages
import java.util.Date;
import java.util.Properties;
import java.util.StringTokenizer;

/**
 * Class for listening to WatchdogEvents and for sending them onto an email address
 *
 * @author Stacy Curl
 */
public class EmailWatchdogListener
    implements WatchdogListener
{
    /**
     * Constructor for EmailWatchdogListener
     * @param    args a series of tokens for initialising the EmailWatchdogListener (cludge necessary
     * as the mlet service is cr..poor)
     * <p>Format of args = "toAddress:fromAddress:SMTPMailHost"
     */
    public EmailWatchdogListener(String args)
    {
        StringTokenizer st = new StringTokenizer(args, ":");

//        System.out.println("Num tokens = " + st.countTokens() + " = [" + st + "]");

        String toAddresses = st.nextToken();

//        System.out.println("toAddresses = " + toAddresses);
        String fromAddress = st.nextToken();

//        System.out.println("fromAddress = " + fromAddress);
        String mailHost = st.nextToken();

//        System.out.println("mailHost = " + mailHost);

        init(toAddresses, fromAddress, mailHost);
    }

    /**
     * Constructor for EmailWatchdogListener
     *
     * @param    toAddresses the address to send emails to
     * @param    fromAddress the address to send emails from
     * @param    mailHost the SMTP mail host to use
     */
    public EmailWatchdogListener(String toAddresses, String fromAddress, String mailHost)
    {
        init(toAddresses, fromAddress, mailHost);
    }

    /**
     * Receives WatchdogEvents
     *
     * @param    watchdogEvent the event received
     *
     * @return whether the event was passed onto email successfully
     */
    public boolean receiveEvent(WatchdogEvent watchdogEvent)
    {
        boolean succeeded = false;

        try
        {
            MimeMessage msg = new MimeMessage(Session.getDefaultInstance(m_properties, null));
            msg.setRecipients(Message.RecipientType.TO,
                              InternetAddress.parse(m_toAddresses, false));
            msg.addFrom(InternetAddress.parse(m_fromAddress, false));
            msg.setReplyTo(InternetAddress.parse(m_fromAddress, false));

//            msg.setSubject("WatchdogEvent");
            msg.setText(watchdogEvent.getEvent());

//            msg.setHeader("X-Mailer", m_properties.getProperty(MAILER_NAME));
            msg.setSentDate(new Date());
            Transport.send(msg);

            succeeded = true;
        }
        catch(Exception e)
        {
            succeeded = false;
        }

        return succeeded;
    }

    /**
     * Initialise the EmailWatchdogListener
     *
     * @param    toAddresses the email address to send to
     * @param    fromAddress the email address to send from
     * @param    mailHost the SMTP mail host to use
     */
    private void init(String toAddresses, String fromAddress, String mailHost)
    {
        m_toAddresses = toAddresses;
        m_fromAddress = fromAddress;

        m_properties = new Properties();

        m_properties.setProperty(MAILHOST, mailHost);
    }

    /** The email address to send to */
    private String m_toAddresses;
    /** The email address to send from*/
    private String m_fromAddress;

    /** The property key which refers to the SMTP mail host */
    private static final String MAILHOST = "mail.smtp.host";

//    private static final String MAILER_NAME = "mail.smtp.mailer";

    /** Mail properties */
    private Properties m_properties;
}
