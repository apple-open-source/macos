// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: SessionDump.java,v 1.15.2.7 2003/06/04 04:47:56 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.servlet;
import java.io.IOException;
import java.io.Writer;
import java.util.Date;
import java.util.Enumeration;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import org.mortbay.html.Page;
import org.mortbay.html.TableForm;
import org.mortbay.util.Code;


/* ------------------------------------------------------------ */
/** Test Servlet Sessions.
 *
 * @version $Revision: 1.15.2.7 $
 * @author Greg Wilkins (gregw)
 */
public class SessionDump extends HttpServlet
{
    int redirectCount=0;
    /* ------------------------------------------------------------ */
    String pageType;

    /* ------------------------------------------------------------ */
    public void init(ServletConfig config)
         throws ServletException
    {
        super.init(config);        
    }

    /* ------------------------------------------------------------ */
    public void doPost(HttpServletRequest request,
                       HttpServletResponse response) 
        throws ServletException, IOException
    {
        HttpSession session = request.getSession(false);
        String action = request.getParameter("Action");
        String name =  request.getParameter("Name");
        String value =  request.getParameter("Value");
        String age =  request.getParameter("MaxAge");

        String nextUrl = request.getRequestURI()+"?R="+redirectCount++;
        if (action.equals("New Session"))
        {   
            session = request.getSession(true);
        }
        else 
        if (session!=null)
        {
            if (action.equals("Invalidate"))
                session.invalidate();
            else if (action.equals("Set"))
            {   
                session.setAttribute(name,value);
                try
                {
                    int m = Integer.parseInt(age);
                    session.setMaxInactiveInterval(m);
                }
                catch(Exception e)
                {
                    Code.ignore(e);
                }
            }
            else if (action.equals("Remove"))
                session.removeAttribute(name);
        }

        String encodedUrl=response.encodeRedirectURL(nextUrl);
        response.sendRedirect(encodedUrl);
        
    }
    
        
    /* ------------------------------------------------------------ */
    public void doGet(HttpServletRequest request,
                      HttpServletResponse response) 
        throws ServletException, IOException
    {
        response.setContentType("text/html");
        Page page= new Page();
        
        page.title("Session Dump Servlet");        
        
        HttpSession session = request.getSession(request.getRequestURI().indexOf("new")>0);
        
        TableForm tf =
            new TableForm(response.encodeURL(request.getRequestURI()));
        tf.method("POST");
        
        if (session==null)
        {
            page.add("<H1>No Session</H1>");
            tf.addButton("Action","New Session");
        }
        else
        {
            try
            {
                tf.addText("ID",session.getId());
                tf.addText("State",session.isNew()?"NEW":"Valid");
                tf.addText("Creation",
                           new Date(session.getCreationTime()).toString());
                tf.addText("Last Access",
                           new Date(session.getLastAccessedTime()).toString());
                tf.addText("Max Inactive",
                           ""+session.getMaxInactiveInterval());

                Enumeration keys=session.getAttributeNames();
                while(keys.hasMoreElements())
                {
                    String name=(String)keys.nextElement();
                    String value=session.getAttribute(name).toString();
                    tf.addText(name,value);
                }
                
                tf.addTextField("Name","Property Name",20,"name");
                tf.addTextField("Value","Property Value",20,"value");
                tf.addTextField("MaxAge","MaxAge(s)",5,"");
                tf.addButtonArea();
                tf.addButton("Action","Set");
                tf.addButton("Action","Remove");
                tf.addButton("Action","Invalidate");

                page.add(tf);
                tf=null;
                if (request.isRequestedSessionIdFromCookie())
                    page.add("<P>Turn off cookies in your browser to try url encoding<BR>");
                
                if (request.isRequestedSessionIdFromURL())
                    page.add("<P>Turn on cookies in your browser to try cookie encoding<BR>");
                
            }
            catch (IllegalStateException e)
            {
                Code.debug(e);
                page.add("<H1>INVALID Session</H1>");
                tf=new TableForm(request.getRequestURI());
                tf.addButton("Action","New Session");
            }
        }

        if (tf!=null)
            page.add(tf);
        
        Writer writer=response.getWriter();
        page.write(writer);
        writer.flush();
    }

    /* ------------------------------------------------------------ */
    public String getServletInfo() {
        return "Session Dump Servlet";
    }
}
