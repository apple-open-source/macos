package org.jboss.test.web.servlets;           

import java.util.Collection;
import java.util.Iterator;

import java.io.IOException;
import java.io.PrintWriter;

import javax.naming.InitialContext;
import javax.naming.NamingException;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import javax.transaction.UserTransaction;

import org.jboss.test.cts.interfaces.CtsBmpHome;
import org.jboss.test.cts.interfaces.CtsBmp;
import org.jboss.test.cts.interfaces.UserTransactionTester;


/**
 *  A servlet that tests UserTransaction support.
 *
 *  Adapted from Scott Starks EJBServlet.java.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public class UserTransactionServlet extends HttpServlet
{
    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        // Get an initial context
        InitialContext ctx;
        try {
            ctx = new InitialContext();
        } catch (NamingException ex) {
            throw new ServletException("Unable to get an InitialContext", ex);
        }

        // Get the UserTransaction
        UserTransaction ut;
        try {
            ut = (UserTransaction)ctx.lookup("java:comp/UserTransaction");
        } catch (NamingException ex) {
            throw new ServletException("Unable to lookup UserTransaction", ex);
        }

        // Get the UserTransaction test bean home
        CtsBmpHome home;
        try {
            home = (CtsBmpHome)ctx.lookup("java:comp/env/ejb/CtsBmp");
        } catch (NamingException ex) {
            throw new ServletException("Unable to lookup CtsBmp home", ex);
        }

        // Initialize the UserTransaction test home to empty
        try {
            // The findAll() implementation has the side-effect of
            // creating the database table, if it does not exist.
            Collection clct = home.findAll();

            // Remove any old beans
            if (clct.size() != 0) {
                for (Iterator itr=clct.iterator(); itr.hasNext();) {
                    CtsBmp bean = (CtsBmp)itr.next();
                    bean.remove();
	        }
            }
        } catch (Exception ex) {
            throw new ServletException("Unable to initialize CtsBmp", ex);
        }

        // Create an UT tester instance
        UserTransactionTester utt = new UserTransactionTester(home, ut);
 
        if (!utt.runAllTests())
            throw new ServletException("UserTransaction tests FAILED");

        response.setContentType("text/html");
        PrintWriter out = response.getWriter();
        out.println("<html>");
        out.println("<head><title>UserTransactionServlet</title></head>");
        out.println("<body>UserTransaction tests passed</body>");
        out.println("</html>");
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
}
