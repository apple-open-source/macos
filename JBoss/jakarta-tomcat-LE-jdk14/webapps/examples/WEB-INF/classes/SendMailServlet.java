/* $Id: SendMailServlet.java,v 1.1 2001/09/09 04:00:08 craigmcc Exp $
 *
 */

import java.io.IOException;
import java.io.PrintWriter;
import javax.mail.Message;
import javax.mail.Session;
import javax.mail.Transport;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMessage;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;



/**
 * Example servlet sending mail message via JNDI resource.
 *
 * @author Craig McClanahan
 * @version $Revision: 1.1 $ $Date: 2001/09/09 04:00:08 $
 */

public class SendMailServlet extends HttpServlet {

    public void doPost(HttpServletRequest request,
                       HttpServletResponse response)
        throws IOException, ServletException
    {

        // Acquire request parameters we need
        String from = request.getParameter("mailfrom");
        String to = request.getParameter("mailto");
        String subject = request.getParameter("mailsubject");
        String content = request.getParameter("mailcontent");
        if ((from == null) || (to == null) ||
            (subject == null) || (content == null)) {
            RequestDispatcher rd =
                getServletContext().getRequestDispatcher("/jsp/mail/sendmail.jsp");
            rd.forward(request, response);
            return;
        }

        // Prepare the beginning of our response
        PrintWriter writer = response.getWriter();
        response.setContentType("text/html");
        writer.println("<html>");
        writer.println("<head>");
        writer.println("<title>Example Mail Sending Results</title>");
        writer.println("</head>");
        writer.println("<body bgcolor=\"white\">");

        try {

            // Acquire our JavaMail session object
            Context initCtx = new InitialContext();
            Context envCtx = (Context) initCtx.lookup("java:comp/env");
            Session session = (Session) envCtx.lookup("mail/Session");

            // Prepare our mail message
            Message message = new MimeMessage(session);
            message.setFrom(new InternetAddress(from));
            InternetAddress dests[] = new InternetAddress[]
                { new InternetAddress(to) };
            message.setRecipients(Message.RecipientType.TO, dests);
            message.setSubject(subject);
            message.setContent(content, "text/plain");

            // Send our mail message
            Transport.send(message);

            // Report success
            writer.println("<strong>Message successfully sent!</strong>");

        } catch (Throwable t) {

            writer.println("<font color=\"red\">");
            writer.println("ENCOUNTERED EXCEPTION:  " + t);
            writer.println("<pre>");
            t.printStackTrace(writer);
            writer.println("</pre>");
            writer.println("</font>");

        }

        // Prepare the ending of our response
        writer.println("<br><br>");
        writer.println("<a href=\"jsp/mail/sendmail.jsp\">Create a new message</a><br>");
        writer.println("<a href=\"jsp/index.html\">Back to examples home</a><br>");
        writer.println("</body>");
        writer.println("</html>");        

    }

}
