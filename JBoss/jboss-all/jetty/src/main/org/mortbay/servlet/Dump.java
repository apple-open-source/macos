// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Dump.java,v 1.15.2.12 2003/07/11 00:55:03 jules_gosnell Exp $
// ---------------------------------------------------------------------------

package org.mortbay.servlet;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.util.Enumeration;
import java.util.Locale;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.UnavailableException;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import org.mortbay.html.Break;
import org.mortbay.html.Font;
import org.mortbay.html.Heading;
import org.mortbay.html.Page;
import org.mortbay.html.Select;
import org.mortbay.html.Table;
import org.mortbay.html.TableForm;
import org.mortbay.http.HttpException;
import org.mortbay.util.Code;
import org.mortbay.util.Loader;

/* ------------------------------------------------------------ */
/** Dump Servlet Request.
 * 
 */
public class Dump extends HttpServlet
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
    public void doPost(HttpServletRequest request, HttpServletResponse response) 
        throws ServletException, IOException
    {
        doGet(request,response);
    }
    
    /* ------------------------------------------------------------ */
    public void doGet(HttpServletRequest request, HttpServletResponse response) 
        throws ServletException, IOException
    {
        String info=request.getPathInfo();
        if (info!=null && info.endsWith("Exception"))
        {
            try
            {
                throw (Throwable)(Loader.loadClass(this.getClass(),
                                                   info.substring(1)).newInstance());
            }
            catch(Throwable th)
            {
                throw new ServletException(th);
            }
        }

        String redirect=request.getParameter("redirect");
        if (redirect!=null && redirect.length()>0)
        {
            response.sendRedirect(redirect);
            return;
        }
        
        String buffer=request.getParameter("buffer");
        if (buffer!=null && buffer.length()>0)
            response.setBufferSize(Integer.parseInt(buffer));
        
        request.setCharacterEncoding("UTF-8");
        response.setContentType("text/html");

        if (info!=null && info.indexOf("Locale/")>=0)
        {
            try
            {
                String locale_name=info.substring(info.indexOf("Locale/")+7);
                Field f=java.util.Locale.class.getField(locale_name);
                response.setLocale((Locale)f.get(null));
            }
            catch(Exception e)
            {
                Code.ignore(e);
                response.setLocale(Locale.getDefault());
            }
        }

        if (info!=null && info.indexOf("Cookie")>=0)
        {
            Cookie cookie = new Cookie("Cookie",info);
            cookie.setMaxAge(300);
            cookie.setPath("/");
            cookie.setComment("Cookie from dump servlet");
            if (info.indexOf("Cookie1")>=0)
                cookie.setVersion(1);
            if (info.indexOf("Cookie2")>=0)
                cookie.setVersion(2);
            response.addCookie(cookie);
        }

        String pi=request.getPathInfo();
        if (pi!=null && pi.startsWith("/ex"))
        {
            OutputStream out = response.getOutputStream();
            out.write("</H1>This text should be reset</H1>".getBytes());
            if ("/ex0".equals(pi))
                throw new ServletException("test ex0",new Throwable());
            if ("/ex1".equals(pi))
                throw new IOException("test ex1");
            if ("/ex2".equals(pi))
                throw new UnavailableException("test ex2");
            if ("/ex3".equals(pi))
                throw new HttpException(501);
        }
        
        
        PrintWriter pout = response.getWriter();
        Page page=null;

        try{
            page = new Page();
            page.title("Dump Servlet");     

            page.add(new Heading(1,"Dump Servlet"));
            Table table = new Table(0).cellPadding(0).cellSpacing(0);
            page.add(table);
            table.newRow();
            table.addHeading("getMethod:&nbsp;").cell().right();
            table.addCell(""+request.getMethod());
            table.newRow();
            table.addHeading("getContentLength:&nbsp;").cell().right();
            table.addCell(Integer.toString(request.getContentLength()));
            table.newRow();
            table.addHeading("getContentType:&nbsp;").cell().right();
            table.addCell(""+request.getContentType());
            table.newRow();
            table.addHeading("getRequestURI:&nbsp;").cell().right();
            table.addCell(""+request.getRequestURI());
            table.newRow();
            table.addHeading("getRequestURL:&nbsp;").cell().right();
            table.addCell(""+request.getRequestURL());
            table.newRow();
            table.addHeading("getContextPath:&nbsp;").cell().right();
            table.addCell(""+request.getContextPath());
            table.newRow();
            table.addHeading("getServletPath:&nbsp;").cell().right();
            table.addCell(""+request.getServletPath());
            table.newRow();
            table.addHeading("getPathInfo:&nbsp;").cell().right();
            table.addCell(""+request.getPathInfo());
            table.newRow();
            table.addHeading("getPathTranslated:&nbsp;").cell().right();
            table.addCell(""+request.getPathTranslated());
            table.newRow();
            table.addHeading("getQueryString:&nbsp;").cell().right();
            table.addCell(""+request.getQueryString());

            
            
            table.newRow();
            table.addHeading("getProtocol:&nbsp;").cell().right();
            table.addCell(""+request.getProtocol());
            table.newRow();
            table.addHeading("getScheme:&nbsp;").cell().right();
            table.addCell(""+request.getScheme());
            table.newRow();
            table.addHeading("getServerName:&nbsp;").cell().right();
            table.addCell(""+request.getServerName());
            table.newRow();
            table.addHeading("getServerPort:&nbsp;").cell().right();
            table.addCell(""+Integer.toString(request.getServerPort()));
            table.newRow();
            table.addHeading("getRemoteUser:&nbsp;").cell().right();
            table.addCell(""+request.getRemoteUser());
            table.newRow();
            table.addHeading("getRemoteAddr:&nbsp;").cell().right();
            table.addCell(""+request.getRemoteAddr());
            table.newRow();
            table.addHeading("getRemoteHost:&nbsp;").cell().right();
            table.addCell(""+request.getRemoteHost());            
            table.newRow();
            table.addHeading("getRequestedSessionId:&nbsp;").cell().right();
            table.addCell(""+request.getRequestedSessionId());                                           
            table.newRow();
            table.addHeading("isSecure():&nbsp;").cell().right();
            table.addCell(""+request.isSecure());
            
            table.newRow();
            table.addHeading("isUserInRole(dumpRole):&nbsp;").cell().right();
            table.addCell(""+request.isUserInRole("dumpRole"));
            
            table.newRow();
            table.addHeading("getLocale:&nbsp;").cell().right();
            table.addCell(""+request.getLocale());
            
            Enumeration locales = request.getLocales();
            while(locales.hasMoreElements())
            {
                table.newRow();
                table.addHeading("getLocales:&nbsp;").cell().right();
                table.addCell(locales.nextElement());
            }

            table.newRow();
            table.newHeading()
                .cell().nest(new Font(2,true))
                .add("<BR>Other HTTP Headers")
                .attribute("COLSPAN","2")
                .left();
            Enumeration h = request.getHeaderNames();
            String name;
            while (h.hasMoreElements())
            {
                name=(String)h.nextElement();

                Enumeration h2=request.getHeaders(name);
                while (h2.hasMoreElements())
                {
                    String hv=(String)h2.nextElement();
                    table.newRow();
                    table.addHeading(name+":&nbsp;").cell().right();
                    table.addCell(hv);
                }
            }
            
            table.newRow();
            table.newHeading()
                .cell().nest(new Font(2,true))
                .add("<BR>Request Parameters")
                .attribute("COLSPAN","2")
                .left();
            h = request.getParameterNames();
            while (h.hasMoreElements())
            {
                name=(String)h.nextElement();
                table.newRow();
                table.addHeading(name+":&nbsp;").cell().right();
                table.addCell(request.getParameter(name));
                String[] values = request.getParameterValues(name);
                if (values==null)
                {
                    table.newRow();
                    table.addHeading(name+" Values:&nbsp;")
                        .cell().right();
                    table.addCell("NULL!!!!!!!!!");
                }
                else
                if (values.length>1)
                {
                    for (int i=0;i<values.length;i++)
                    {
                        table.newRow();
                        table.addHeading(name+"["+i+"]:&nbsp;")
                            .cell().right();
                        table.addCell(values[i]);
                    }
                }
            }
            
	    /* ------------------------------------------------------------ */
            table.newRow();
            table.newHeading()
                .cell().nest(new Font(2,true))
                .add("<BR>Request Attributes")
                .attribute("COLSPAN","2")
                .left();
            Enumeration a = request.getAttributeNames();
            while (a.hasMoreElements())
            {
                name=(String)a.nextElement();
                table.newRow();
                table.addHeading(name+":&nbsp;")
		    .cell().attribute("VALIGN","TOP").right();
		table.addCell("<pre>" +
			      toString(request.getAttribute(name))
			      + "</pre>");
            }

            table.newRow();
            table.newHeading()
                .cell().nest(new Font(2,true))
                .add("<BR>Servlet InitParameters")
                .attribute("COLSPAN","2")
                .left();
            a = getInitParameterNames();
            while (a.hasMoreElements())
            {
                name=(String)a.nextElement();
                table.newRow();
                table.addHeading(name+":&nbsp;")
		    .cell().attribute("VALIGN","TOP").right();
                table.addCell("<pre>" +
			      toString(getInitParameter(name))
			      + "</pre>");
            }
            
            table.newRow();
            table.newHeading()
                .cell().nest(new Font(2,true))
                .add("<BR>Context InitParameters")
                .attribute("COLSPAN","2")
                .left();
            a = getServletContext().getInitParameterNames();
            while (a.hasMoreElements())
            {
                name=(String)a.nextElement();
                table.newRow();
                table.addHeading(name+":&nbsp;")
		    .cell().attribute("VALIGN","TOP").right();
                table.addCell("<pre>" +
			      toString(getServletContext()
				       .getInitParameter(name))
			      + "</pre>");
            }

            table.newRow();
            table.newHeading()
                .cell().nest(new Font(2,true))
                .add("<BR>Context Attributes")
                .attribute("COLSPAN","2")
                .left();
            a = getServletContext().getAttributeNames();
            while (a.hasMoreElements())
            {
                name=(String)a.nextElement();
                table.newRow();
                table.addHeading(name+":&nbsp;")
		    .cell().attribute("VALIGN","TOP").right();
                table.addCell("<pre>" +
			      toString(getServletContext()
				       .getAttribute(name))
			      + "</pre>");
            }

            if (request.getContentType()!=null &&
                request.getContentType().startsWith("multipart/form-data") &&
                request.getContentLength()<1000000)
            {
                MultiPartRequest multi = new MultiPartRequest(request);
                String[] parts = multi.getPartNames();
                
                table.newRow();
                table.newHeading()
                    .cell().nest(new Font(2,true))
                    .add("<BR>Multi-part content")
                    .attribute("COLSPAN","2")
                    .left();
                for (int p=0;p<parts.length;p++)
                {
                    name=parts[p];
                    table.newRow();
                    table.addHeading(name+":&nbsp;")
                        .cell().attribute("VALIGN","TOP").right();
                    table.addCell("<pre>" +
                                  multi.getString(parts[p]) +
                                  "</pre>");
                }
            }
            
            String res=request.getParameter("resource");
            if (res!=null && res.length()>0)
            {
                table.newRow();
                table.newHeading()
                    .cell().nest(new Font(2,true))
                    .add("<BR>Get Resource: "+res)
                    .attribute("COLSPAN","2")
                    .left();
                
                table.newRow();
                table.addHeading("this.getClass():&nbsp;").cell().right();
                table.addCell(""+this.getClass().getResource(res));
                
                table.newRow();
                table.addHeading("this.getClass().getClassLoader():&nbsp;").cell().right();
                table.addCell(""+this.getClass().getClassLoader().getResource(res));
                
                table.newRow();
                table.addHeading("Thread.currentThread().getContextClassLoader():&nbsp;").cell().right();
                table.addCell(""+Thread.currentThread().getContextClassLoader().getResource(res));
                
                table.newRow();
                table.addHeading("getServletContext():&nbsp;").cell().right();
                table.addCell(""+getServletContext().getResource(res));
            }

            
            page.add(Break.para);
            
            page.add(new Heading(1,"Form to generate Dump content"));
            TableForm tf = new TableForm(response.encodeURL(request.getRequestURI()));
            tf.method("POST");
            tf.addTextField("TextField","TextField",20,"value");
            Select select = tf.addSelect("Select","Select",true,3);
            select.add("ValueA");
            select.add("ValueB1,ValueB2");
            select.add("ValueC");
            tf.addButton("Action","Submit");
            page.add(tf);

            page.add(new Heading(1,"Form to upload content"));
            tf = new TableForm(response.encodeURL(request.getRequestURI()));
            tf.method("POST");
            tf.attribute("enctype","multipart/form-data");
            tf.addFileField("file","file");              
            tf.addButton("Upload","Upload");
            page.add(tf);
            
            page.add(new Heading(1,"Form to get Resource"));
            tf = new TableForm(response.encodeURL(request.getRequestURI()));
            tf.method("POST");
            tf.addTextField("resource","resource",20,"");              
            tf.addButton("Action","getResource");
            page.add(tf);

        }
        catch (Exception e)
        {
            Code.warning(e);
        }
    
        page.write(pout);

        String data=request.getParameter("data");
        if (data!=null && data.length()>0)
        {
            int d = Integer.parseInt(data);
            while (d>0)
            {
                pout.println("1234567890123456789012345678901234567890123456789\n");
                d=d-50;

            }
        }
        
        pout.close();
        
        if (pi!=null)
        {
            if ("/ex4".equals(pi))
                throw new ServletException("test ex4",new Throwable());
            if ("/ex5".equals(pi))
                throw new IOException("test ex5");
            if ("/ex6".equals(pi))
                throw new UnavailableException("test ex6");
            if ("/ex7".equals(pi))
                throw new HttpException(501);
        }
        
    }

    /* ------------------------------------------------------------ */
    public String getServletInfo()
    {
        return "Dump Servlet";
    }

    /* ------------------------------------------------------------ */
    public synchronized void destroy()
    {
        Code.debug("Destroyed");
    }
    
    /* ------------------------------------------------------------ */
    private static String toString(Object o)
    {
	if (o == null)
	    return null;

	if (o.getClass().isArray())
	{
	    StringBuffer sb = new StringBuffer();
	    Object[] array = (Object[]) o;
	    for (int i=0; i<array.length; i++)
	    {
		if (i > 0)
		    sb.append("\n");
		sb.append(array.getClass().getComponentType().getName());
		sb.append("[");
		sb.append(i);
		sb.append("]=");
		sb.append(toString(array[i]));
	    }
	    return sb.toString();
	}
	else
	    return o.toString();
    }
    
}
