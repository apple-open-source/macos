package org.jboss.test.web.servlets;           

import java.io.IOException;
import java.io.PrintWriter;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.sql.DataSource;

import org.jboss.test.web.interfaces.ReferenceTest;
import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;
import org.jboss.test.web.interfaces.StatelessSessionLocal;
import org.jboss.test.web.interfaces.StatelessSessionLocalHome;
import org.jboss.test.web.util.Util;

/** A servlet that accesses an EJB and tests the speed of optimized versus non-optimized invocations.

@author  Adrian.Brock@HappeningTimes.com
@version $Revision: 1.1.4.1 $
*/
public class SpeedServlet extends HttpServlet
{
    public static final int REPEATS = 10;
    public static final int ITERATIONS = 100;

    protected long[] runRemoteTest(StatelessSession bean, boolean optimized)
       throws Exception
    {
       ReferenceTest test = new ReferenceTest();
       long[] results = new long[REPEATS];
       for (int loop = 0; loop < REPEATS; loop ++)
       {
          long start = System.currentTimeMillis();
          for (int i = 0; i < ITERATIONS; i++)
             bean.noop(test, optimized);
          results[loop] = System.currentTimeMillis() - start;
       }
       return results;
    }

    protected void displayResults(PrintWriter out, long[] results)
       throws IOException
    {
       long total = 0;
       out.print("<table><tr>");
       for (int i = 0; i < results.length; i++)
       {
          total += results[i];

          out.print("<td>" + results[i] + " ms</td>");
       }
       out.println("</tr></table><br />");
       out.println("Total time  = " + total + " ms<br />");
       out.println("Invocations = " + ITERATIONS * REPEATS);
       float average = total * 1000;
       average /= (ITERATIONS * REPEATS);
       out.println("Average time= " + average + " micro-seconds<br />");
    }

    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        long[] optimized = null;
        long[] notOptimized = null;
        try
        {
            InitialContext ctx = new InitialContext();
            Context enc = (Context) ctx.lookup("java:comp/env");
            StatelessSessionHome home = (StatelessSessionHome) enc.lookup("ejb/OptimizedEJB");
            StatelessSession bean = home.create();
            optimized = runRemoteTest(bean, true);

            home = (StatelessSessionHome) enc.lookup("ejb/NotOptimizedEJB");
            bean = home.create();
            notOptimized = runRemoteTest(bean, false);
        }
        catch(Exception e)
        {
            throw new ServletException("Failed to run speed tests", e);
        }
        response.setContentType("text/html");
        PrintWriter out = response.getWriter();
        out.println("<html>");
        out.println("<head><title>SpeedServlet</title></head>");
        out.println("<body>");
        out.println("Number of invocations=" + ITERATIONS + " repeated " + REPEATS + " times.<br />");
        out.println("<h2>ejb/OptimizedEJB</h2>");
        displayResults(out, optimized);
        out.println("<h2>ejb/NotOptimizedEJB</h2>");
        displayResults(out, notOptimized);
        out.println("</body>");
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
