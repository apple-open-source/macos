/* $Id: JndiServlet.java,v 1.2 2001/11/08 22:02:16 remm Exp $
 *
 */

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Enumeration;
import java.util.Hashtable;
import javax.servlet.*;
import javax.servlet.http.*;
import javax.naming.NamingException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingEnumeration;
import javax.naming.directory.InitialDirContext;

/**
 * Demonstration of the web application environment support.
 *
 * @author Remy Maucherat
 */

public class JndiServlet 
    extends HttpServlet {


    public void doGet(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException {
        
        PrintWriter out = response.getWriter();
        response.setContentType("text/plain");
        
        Context ctx = null;
        
        try {
            ctx = new InitialContext();
        } catch (NamingException e) {
            out.println("Couldn't build an initial context : " + e);
            return;
        }
        
        try {
            Object value = ctx.lookup("java:/comp/env/maxExemptions");
            out.println("Simple lookup test : ");
            out.println("Max exemptions value : " + value);
        } catch (NamingException e) {
            out.println("JNDI lookup failed : " + e);
        }
        
        try {
            Object value = ctx.lookup("java:/comp/env/linkToGlobalResource");
            out.println("Resource link test : ");
            out.println("Link value : " + value);
        } catch (NamingException e) {
            out.println("JNDI lookup failed : " + e);
        }
        
        try {
            Context envCtx = (Context) ctx.lookup("java:/comp/env/");
            out.println("list() on /comp/env Context : ");
            NamingEnumeration enum = ctx.list("java:/comp/env/");
            while (enum.hasMoreElements()) {
                out.print("Binding : ");
                out.println(enum.nextElement().toString());
            }
            out.println("listBindings() on /comp/env Context : ");
            enum = ctx.listBindings("java:/comp/env/");
            while (enum.hasMoreElements()) {
                out.print("Binding : ");
                out.println(enum.nextElement().toString());
            }
        } catch (NamingException e) {
            out.println("JNDI lookup failed : " + e);
        }
        
    }
    
    
}

