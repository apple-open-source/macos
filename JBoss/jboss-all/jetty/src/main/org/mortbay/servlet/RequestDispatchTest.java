// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: RequestDispatchTest.java,v 1.16.2.5 2003/06/04 04:47:56 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.servlet;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletRequestWrapper;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpServletResponseWrapper;
import org.mortbay.util.Code;
import org.mortbay.util.StringUtil;


/* ------------------------------------------------------------ */
/** Test Servlet RequestDispatcher.
 * 
 * @version $Id: RequestDispatchTest.java,v 1.16.2.5 2003/06/04 04:47:56 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class RequestDispatchTest extends HttpServlet
{
    /* ------------------------------------------------------------ */
    String pageType;

    /* ------------------------------------------------------------ */
    public void init(ServletConfig config)
         throws ServletException
    {
        super.init(config);
    }

    /* ------------------------------------------------------------ */
    public void doPost(HttpServletRequest sreq, HttpServletResponse sres) 
        throws ServletException, IOException
    {
        doGet(sreq,sres);
    }
    
    /* ------------------------------------------------------------ */
    public void doGet(HttpServletRequest sreq, HttpServletResponse sres) 
        throws ServletException, IOException
    {
        sreq=new HttpServletRequestWrapper(sreq);
        sres=new HttpServletResponseWrapper(sres);
        
        String prefix = sreq.getContextPath()!=null
            ? sreq.getContextPath()+sreq.getServletPath()
            : sreq.getServletPath();
        
        sres.setContentType("text/html");

        String info ;

        if (sreq.getAttribute("javax.servlet.include.servlet_path")!=null)
            info=(String)sreq.getAttribute("javax.servlet.include.path_info");
        else
            info=sreq.getPathInfo();
        
        if (info==null)
            info="NULL";

        if (info.startsWith("/include/"))
        {
            info=info.substring(8);
            if (info.indexOf('?')<0)
                info+="?Dispatch=include";
            else
                info+="&Dispatch=include";

            
            if (System.currentTimeMillis()%2==0)
            {
                PrintWriter pout=null;
                pout = sres.getWriter();
                pout.write("<H1>Include: "+info+"</H1><HR>");
                
                RequestDispatcher dispatch = getServletContext()
                    .getRequestDispatcher(info);
                if (dispatch==null)
                {
                    pout = sres.getWriter();
                    pout.write("<H1>Null dispatcher</H1>");
                }
                else
                    dispatch.include(sreq,sres);
                
                pout.write("<HR><H1>-- Included (writer)</H1>");
            }
            else 
            {
                OutputStream out=null;
                out = sres.getOutputStream();
                out.write(("<H1>Include: "+info+"</H1><HR>").getBytes(StringUtil.__ISO_8859_1));   

                RequestDispatcher dispatch = getServletContext()
                    .getRequestDispatcher(info);
                if (dispatch==null)
                {
                    out = sres.getOutputStream();
                    out.write("<H1>Null dispatcher</H1>".getBytes(StringUtil.__ISO_8859_1));
                }
                else
                    dispatch.include(sreq,sres);
                
                out.write("<HR><H1>-- Included (outputstream)</H1>".getBytes(StringUtil.__ISO_8859_1));
            }
        }
        else if (info.startsWith("/forward/"))
        {
            info=info.substring(8);
            if (info.indexOf('?')<0)
                info+="?Dispatch=forward";
            else
                info+="&Dispatch=forward";
            RequestDispatcher dispatch =
                getServletContext().getRequestDispatcher(info);
            if (dispatch!=null)
                dispatch.forward(sreq,sres);
            else
            {
                PrintWriter pout = sres.getWriter();
                pout.write("<H1>No dispatcher for: "+info+"</H1><HR>");
                pout.flush();
            }
        }
        else if (info.startsWith("/forwardC/"))
        {
            info=info.substring(9);
            if (info.indexOf('?')<0)
                info+="?Dispatch=forward";
            else
                info+="&Dispatch=forward";

            String cpath=info.substring(0,info.indexOf('/',1));
            info=info.substring(cpath.length());
 
            RequestDispatcher dispatch =
                getServletContext().getContext(cpath).getRequestDispatcher(info);
            if (dispatch!=null)
                dispatch.forward(sreq,sres);
            else
            {
                PrintWriter pout = sres.getWriter();
                pout.write("<H1>No dispatcher for: "+cpath+"/"+info+"</H1><HR>");
                pout.flush();
            }
        }
        else if (info.startsWith("/includeN/"))
        {
            info=info.substring(10);
            if (info.indexOf("/")>=0)
                info=info.substring(0,info.indexOf("/"));
            
            PrintWriter pout;
            if (info.startsWith("/null"))
                info=info.substring(5);
            else
            {
                pout = sres.getWriter();
                pout.write("<H1>Include named: "+info+"</H1><HR>");
            }
            
            RequestDispatcher dispatch = getServletContext()
                .getNamedDispatcher(info);
            if (dispatch!=null)
                dispatch.include(sreq,sres);
            else
            {
                pout = sres.getWriter();
                pout.write("<H1>No servlet named: "+info+"</H1>");
            }
            
            pout = sres.getWriter();
            pout.write("<HR><H1>Included ");
        }
        else if (info.startsWith("/forwardN/"))
        {
            info=info.substring(10);
            if (info.indexOf("/")>=0)
                info=info.substring(0,info.indexOf("/"));
            RequestDispatcher dispatch = getServletContext()
                .getNamedDispatcher(info);
            if (dispatch!=null)
                dispatch.forward(sreq,sres);
            else
            {
                PrintWriter pout = sres.getWriter();
                pout.write("<H1>No servlet named: "+info+"</H1>");
                pout.flush();
            }
        }
        else
        {
            PrintWriter pout = sres.getWriter();
            pout.write("<H1>Dispatch URL must be of the form: </H1>"+
                       "<PRE>"+prefix+"/include/path\n"+
                       prefix+"/forward/path\n"+
                       prefix+"/includeN/name\n"+
                       prefix+"/forwardN/name\n"+
                       prefix+"/forwardC/context/path</PRE>"
                       );
            pout.flush();
        }
    }

    /* ------------------------------------------------------------ */
    public String getServletInfo()
    {
        return "Include Servlet";
    }

    /* ------------------------------------------------------------ */
    public synchronized void destroy()
    {
        Code.debug("Destroyed");
    }
    
}
