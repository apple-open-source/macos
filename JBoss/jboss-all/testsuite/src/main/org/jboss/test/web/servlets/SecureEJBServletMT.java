package org.jboss.test.web.servlets;           

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.security.Principal;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;

/** A servlet that spawns a thread to perform a long running task that
interacts with a secure EJB.

@author  Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public class SecureEJBServletMT extends HttpServlet
{
   static org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(SecureEJBServletMT.class);
   
    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        HttpSession session = request.getSession();
        Principal user = request.getUserPrincipal();
        Object result = session.getAttribute("request.result");

        response.setContentType("text/html");
        PrintWriter out = response.getWriter();
        out.println("<html>");
        out.println("<head><title>SecureEJBServletMT</title></head>");
        if( result == null )
            out.println("<meta http-equiv='refresh' content='5'>");
        out.println("<h1>SecureEJBServletMT Accessed</h1>");
        out.println("<body><pre>You have accessed this servlet as user: "+user);

        if( result == null )
        {
            Worker worker = new Worker(session);
            out.println("Started worker thread...");
            Thread t = new Thread(worker, "Worker");
            t.start();
        }
        else if( result instanceof Exception )
        {
            StringWriter sw = new StringWriter();
            PrintWriter pw = new PrintWriter(sw);
            Exception e = (Exception) result;
            e.printStackTrace(pw);
            response.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, sw.toString());
        }
        else
        {
            out.println("Finished request, result = "+result);
        }
        out.println("</pre></body></html>");
        out.close();
    }

    protected void doGet(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        processRequest(request, response);
    }

    protected void doPost(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        processRequest(request, response);
    }

    static class Worker implements Runnable
    {
        HttpSession session;
        Worker(HttpSession session)
        {
            this.session = session;
        }
        public void run()
        {
            try
            {
                log.debug("Worker, start: "+System.currentTimeMillis());
                Thread.currentThread().sleep(2500);
                InitialContext ctx = new InitialContext();
                StatelessSessionHome home = home = (StatelessSessionHome) ctx.lookup("java:comp/env/ejb/SecuredEJB");
                StatelessSession bean = home.create();
                String echoMsg = bean.echo("SecureEJBServlet called SecuredEJB.echo");
                session.setAttribute("request.result", echoMsg);
            }
            catch(Exception e)
            {
                session.setAttribute("request.result", e);
            }
            finally
            {
                log.debug("Worker, end: "+System.currentTimeMillis());
            }
        }
    }
}
