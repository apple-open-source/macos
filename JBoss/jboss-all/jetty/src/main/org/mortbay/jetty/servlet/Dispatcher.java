// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Dispatcher.java,v 1.17.2.9 2003/07/11 00:55:03 jules_gosnell Exp $
// ---------------------------------------------------------------------------

package org.mortbay.jetty.servlet;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletException;
import javax.servlet.ServletOutputStream;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletRequestWrapper;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpServletResponseWrapper;
import javax.servlet.http.HttpSession;
import org.mortbay.http.HttpConnection;
import org.mortbay.http.PathMap;
import org.mortbay.util.Code;
import org.mortbay.util.LazyList;
import org.mortbay.util.MultiMap;
import org.mortbay.util.StringMap;
import org.mortbay.util.URI;
import org.mortbay.util.UrlEncoded;
import org.mortbay.util.WriterOutputStream;


/* ------------------------------------------------------------ */
/** Servlet RequestDispatcher.
 * 
 * @version $Id: Dispatcher.java,v 1.17.2.9 2003/07/11 00:55:03 jules_gosnell Exp $
 * @author Greg Wilkins (gregw)
 */
public class Dispatcher implements RequestDispatcher
{
    public final static String __REQUEST_URI= "javax.servlet.include.request_uri";
    public final static String __CONTEXT_PATH= "javax.servlet.include.context_path";
    public final static String __SERVLET_PATH= "javax.servlet.include.servlet_path";
    public final static String __PATH_INFO= "javax.servlet.include.path_info";
    public final static String __QUERY_STRING= "javax.servlet.include.query_string";
    public final static StringMap __managedAttributes = new StringMap();
    static
    {
        __managedAttributes.put(__REQUEST_URI,__REQUEST_URI);
        __managedAttributes.put(__CONTEXT_PATH,__CONTEXT_PATH);
        __managedAttributes.put(__SERVLET_PATH,__SERVLET_PATH);
        __managedAttributes.put(__PATH_INFO,__PATH_INFO);
        __managedAttributes.put(__QUERY_STRING,__QUERY_STRING);
    }
    
    ServletHandler _servletHandler;
    ServletHolder _holder=null;
    String _pathSpec;
    String _uriInContext;
    String _pathInContext;
    String _query;
    boolean _include;
    DispatcherRequest _request;
    boolean _xContext;
    HttpSession _xSession;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
    /** Constructor. 
     * @param servletHandler 
     * @param uriInContext Encoded uriInContext
     * @param pathInContext Encoded pathInContext
     * @param query
     * @exception IllegalStateException 
     */
    Dispatcher(ServletHandler servletHandler,
               String uriInContext,
               String pathInContext,
               String query,
               Map.Entry entry)
        throws IllegalStateException
    {
        Code.debug("Dispatcher for ",servletHandler,",",uriInContext,",",query);
        
        _servletHandler=servletHandler;
        _uriInContext=uriInContext;
        _pathInContext=pathInContext;        
        _query=query;
        _pathSpec=(String)entry.getKey();
        _holder = (ServletHolder)entry.getValue();
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param servletHandler
     * @param name
     */
    Dispatcher(ServletHandler servletHandler,String name)
        throws IllegalStateException
    {
        _servletHandler=servletHandler;
        _holder=_servletHandler.getServletHolder(name);
        if (_holder==null)
            throw new IllegalStateException("No named servlet handler in context");
    }

    /* ------------------------------------------------------------ */
    public boolean isNamed()
    {
        return _pathInContext==null;
    }
    
    /* ------------------------------------------------------------ */
    public void include(ServletRequest servletRequest,
                        ServletResponse servletResponse)
        throws ServletException, IOException     
    {
        _include=true;
        dispatch(servletRequest,servletResponse);
    }
    
    /* ------------------------------------------------------------ */
    public void forward(ServletRequest servletRequest,
                        ServletResponse servletResponse)
        throws ServletException,IOException
    {
        dispatch(servletRequest,servletResponse);
    }
    
    /* ------------------------------------------------------------ */
    void dispatch(ServletRequest servletRequest,
                  ServletResponse servletResponse)
        throws ServletException,IOException
    {
        HttpServletRequest httpServletRequest=(HttpServletRequest)servletRequest;
        HttpServletResponse httpServletResponse=(HttpServletResponse)servletResponse;

        HttpConnection httpConnection=
            _servletHandler.getHttpContext().getHttpConnection();
        ServletHttpRequest servletHttpRequest= (httpConnection!=null)
            ?(ServletHttpRequest)httpConnection.getRequest().getWrapper()
            :ServletHttpRequest.unwrap(servletRequest);

        // Is this being dispatched to a different context?
        _xContext=
            servletHttpRequest.getServletHandler()!=_servletHandler;

        // wrap the request and response
        DispatcherRequest request = new DispatcherRequest(httpServletRequest);
        DispatcherResponse response = new DispatcherResponse(httpServletResponse);
        _request=request;
        
        if (!_include)
            servletResponse.resetBuffer();
        
        // Merge parameters
        String query=_query;
        MultiMap parameters=null;
        if (query!=null)
        {
            // Add the parameters
            parameters=new MultiMap();
            UrlEncoded.decodeTo(query,parameters);
            request.addParameters(parameters);
        }
        
        if (isNamed())
        {
            // No further modifications required.
            _servletHandler.dispatch(null,request,response,_holder);
        }
        else
        {
            // merge query string
            String oldQ=httpServletRequest.getQueryString();
            if (oldQ!=null && oldQ.length()>0)
            {
                if (parameters!=null)
                {
                    UrlEncoded encoded = new UrlEncoded(oldQ);
                    Iterator iter = parameters.entrySet().iterator();
                    while(iter.hasNext())
                    {
                        Map.Entry entry=(Map.Entry)iter.next();
                        encoded.addValues(entry.getKey(),
                                          LazyList.getList(entry.getValue(),true));
                    }
                    query=encoded.encode();
                }
                else 
                    query=oldQ;
            }
            
            // Adjust servlet paths
            servletHttpRequest.setServletHandler(_servletHandler);
            request.setPaths(_servletHandler.getHttpContext().getContextPath(),
                             PathMap.pathMatch(_pathSpec,_pathInContext),
                             PathMap.pathInfo(_pathSpec,_pathInContext),
                             query);
            _servletHandler.dispatch(_pathInContext,request,response,_holder);
            
            if (!_include)
                response.close();
            else if (response.isFlushNeeded())
                response.flushBuffer();
        }
            
    }

    /* ------------------------------------------------------------ */
    public String toString()
    {
        return "Dispatcher["+_pathSpec+","+_holder+"]";
    }
        

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    class DispatcherRequest extends HttpServletRequestWrapper
    {
        String _contextPath;
        String _servletPath;
        String _pathInfo;
        String _query;
        MultiMap _parameters;
        HashMap _attributes;
        
        /* ------------------------------------------------------------ */
        DispatcherRequest(HttpServletRequest request)
        {
            super(request);
        }

        /* ------------------------------------------------------------ */
        void setPaths(String cp,String sp, String pi, String qs)
        {
            _contextPath = (cp.length()==1 && cp.charAt(0)=='/')?"":cp;
            _servletPath=sp;
            _pathInfo=pi;
            _query=qs;
        }
        
        /* ------------------------------------------------------------ */
        int getFilterType()
        {
            return _include?FilterHolder.__INCLUDE:FilterHolder.__FORWARD;
        }

        /* ------------------------------------------------------------ */
        String getPathInContext()
        {
            if (_pathInContext!=null)
                return _pathInContext;
            else
                return URI.addPaths(getServletPath(),getPathInfo());
        }
        
        /* ------------------------------------------------------------ */
        public String getRequestURI()
        {
            if (_include || isNamed())
                return super.getRequestURI();
            return URI.addPaths(_contextPath,_uriInContext);
        }
        
        /* ------------------------------------------------------------ */
        public StringBuffer getRequestURL()
        {
            if (_include || isNamed())
                return super.getRequestURL();
            StringBuffer buf = getRootURL();
            if (_contextPath.length()>0)
                buf.append(_contextPath);
            buf.append(_uriInContext);
            return buf;
        }

        
        /* ------------------------------------------------------------ */
        public String getPathTranslated()
        {
            return getRealPath(getPathInContext());
        }
        
        /* ------------------------------------------------------------ */
        StringBuffer getRootURL()
        {
            StringBuffer buf = super.getRequestURL();
            int d=3;
            for (int i=0;i<buf.length();i++)
            {
                if (buf.charAt(i)=='/' && --d==0)
                {
                    buf.setLength(i);
                    break;
                }
            }
            return buf;
        }
        
        /* ------------------------------------------------------------ */
        public String getContextPath()
        {
            return(_include||isNamed())?super.getContextPath():_contextPath;
        }
        
        /* ------------------------------------------------------------ */
        public String getServletPath()
        {
            return(_include||isNamed())?super.getServletPath():_servletPath;
        }
        
        /* ------------------------------------------------------------ */
        public String getPathInfo()
        {
            return(_include||isNamed())?super.getPathInfo():_pathInfo;
        }
        
        /* ------------------------------------------------------------ */
        public String getQueryString()
        {
            return(_include||isNamed())?super.getQueryString():_query;
        }
        

        /* ------------------------------------------------------------ */
        void addParameters(MultiMap parameters)
        {
            _parameters=parameters;
        }
        
        /* -------------------------------------------------------------- */
        public Enumeration getParameterNames()
        {
            if (_parameters==null)
                return super.getParameterNames();
            
            HashSet set = new HashSet(_parameters.keySet());
            Enumeration e = super.getParameterNames();
            while (e.hasMoreElements())
                set.add(e.nextElement());

            return Collections.enumeration(set);
        }
        
        /* -------------------------------------------------------------- */
        public String getParameter(String name)
        {
            if (_parameters==null)
                return super.getParameter(name);
            String value=_parameters.getString(name);
            if (value!=null)
                return value;
            return super.getParameter(name);
        }
        
        /* -------------------------------------------------------------- */
        public String[] getParameterValues(String name)
        {
            String[] v0=super.getParameterValues(name);
            if (_parameters==null)
                return v0;
            List v1=_parameters.getValues(name);

            if (v0==null && v1==null)
                return null;
            
            String[] a=new String[(v0==null?0:v0.length)+(v1==null?0:v1.size())];
            if (v0==null || v0.length==0)
                return (String[])v1.toArray(a);
            if (v1==null || v1.size()==0)
                return v0;
            
            for (int i=0;i<v0.length;i++)
                a[i]=v0[i];
            for (int i=0;i<v1.size();i++)
                a[v0.length+i]=(String)v1.get(i);
            return a;
        }
        
        /* -------------------------------------------------------------- */
        public Map getParameterMap()
        {       
            if (_parameters==null)
                return super.getParameterMap();
            
            Map m0 = super.getParameterMap();
            if (m0==null || m0.size()==0)
                return _parameters.toStringArrayMap();

            Enumeration p = getParameterNames();
            Map m = new HashMap();
            while(p.hasMoreElements())
            {
                String name=(String)p.nextElement();
                m.put(name,getParameterValues(name));
            }
            
            return m;
        }

        /* ------------------------------------------------------------ */
        public void setAttribute(String name, Object value)
        {
            if (__managedAttributes.containsKey(name))
            {
                if (_attributes==null)
                    _attributes=new HashMap(3);
                _attributes.put(name,value);
            }
            else
                super.setAttribute(name,value);
        }
        
        /* ------------------------------------------------------------ */
        public Object getAttribute(String name)
        {
            if (_attributes!=null && _attributes.containsKey(name))
                return _attributes.get(name);
                
            if (_include && !isNamed())
            {
                if (name.equals(__PATH_INFO))    return _pathInfo;
                if (name.equals(__REQUEST_URI))  return URI.addPaths(_contextPath,_uriInContext);
                if (name.equals(__SERVLET_PATH)) return _servletPath;
                if (name.equals(__CONTEXT_PATH)) return _contextPath;
                if (name.equals(__QUERY_STRING)) return _query;
            }
            else
            {
                if (name.equals(__PATH_INFO))    return null;
                if (name.equals(__REQUEST_URI))  return null;
                if (name.equals(__SERVLET_PATH)) return null;
                if (name.equals(__CONTEXT_PATH)) return null;
                if (name.equals(__QUERY_STRING)) return null;
            }
            
            return super.getAttribute(name);
        }
        
        /* ------------------------------------------------------------ */
        public Enumeration getAttributeNames()
        {
            HashSet set=new HashSet();
            Enumeration e=super.getAttributeNames();
            while (e.hasMoreElements())
                set.add(e.nextElement());
            
            if (_include && !isNamed())
            {
                set.add(__PATH_INFO);
                set.add(__REQUEST_URI);
                set.add(__SERVLET_PATH);
                set.add(__CONTEXT_PATH);
                set.add(__QUERY_STRING);
            }
            else
            {
                set.remove(__PATH_INFO);
                set.remove(__REQUEST_URI);
                set.remove(__SERVLET_PATH);
                set.remove(__CONTEXT_PATH);
                set.remove(__QUERY_STRING);
            }
            
            if (_attributes!=null)
                set.addAll(_attributes.keySet());
            
            return Collections.enumeration(set);
        }
        
        /* ------------------------------------------------------------ */
        public HttpSession getSession(boolean create)
        {
            if (_xContext)
            {
                if (_xSession==null)
                {
                    Code.debug("Ctx dispatch session");
                    _xSession=_servletHandler.getHttpSession(getRequestedSessionId());
                    if (create && _xSession==null)
                        _xSession=_servletHandler.newHttpSession((HttpServletRequest)getRequest());
                }
                return _xSession;
            }
            return super.getSession(create);
        }
    
        /* ------------------------------------------------------------ */
        public HttpSession getSession()
        {
            return getSession(true);
        }
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    class DispatcherResponse extends HttpServletResponseWrapper
    {
        private ServletOutputStream _out=null;
        private PrintWriter _writer=null;
        private boolean _flushNeeded=false;
        
        /* ------------------------------------------------------------ */
        DispatcherResponse(HttpServletResponse response)
        {
            super(response);
        }

        /* ------------------------------------------------------------ */
        public ServletOutputStream getOutputStream()
            throws IOException
        {
            if (_writer!=null)
                throw new IllegalStateException("getWriter called");

            if (_out==null)
            {
                try {_out=super.getOutputStream();}
                catch(IllegalStateException e)
                {
                    Code.ignore(e);
                    _flushNeeded=true;
                    _out=new ServletOut(new WriterOutputStream(super.getWriter()));
                }
            }

            if (_include)
                _out=new DontCloseServletOut(_out);
            
            return _out;
        }  
      
        /* ------------------------------------------------------------ */
        public PrintWriter getWriter()
            throws IOException
        {
            if (_out!=null)
                throw new IllegalStateException("getOutputStream called");

            if (_writer==null)
            {                
                try{_writer=super.getWriter();}
                catch(IllegalStateException e)
                {
                    if (Code.debug()) Code.warning(e);
                    _flushNeeded=true;
                    _writer = new ServletWriter(super.getOutputStream(),
                                                getCharacterEncoding());
                }
            }

            if (_include)
                _writer=new DontCloseWriter(_writer);
            return _writer;
        }

        /* ------------------------------------------------------------ */
        boolean isFlushNeeded()
        {
            return _flushNeeded;
        }
        
        /* ------------------------------------------------------------ */
        public void flushBuffer()
            throws IOException
        {
            if (_writer!=null)
                _writer.flush();
            if (_out!=null)
                _out.flush();
            super.flushBuffer();
        }
        
        /* ------------------------------------------------------------ */
        public void close()
            throws IOException
        {
            if (_writer!=null)
                _writer.close();
            if (_out!=null)
                _out.close();
        }
        
        /* ------------------------------------------------------------ */
        public void setLocale(Locale locale)
        {
            if (!_include) super.setLocale(locale);
        }
        
        /* ------------------------------------------------------------ */
        public void sendError(int status, String message)
            throws IOException
        {
            if (!_include) super.sendError(status,message);
        }
        
        /* ------------------------------------------------------------ */
        public void sendError(int status)
            throws IOException
        {
            if (!_include) super.sendError(status);
        }
        
        /* ------------------------------------------------------------ */
        public void sendRedirect(String url)
            throws IOException
        {
            if (!_include)
            {
                if (!url.startsWith("http:/")&&!url.startsWith("https:/"))
                {
                    StringBuffer buf = _request.getRootURL();
                    
                    if (url.startsWith("/"))
                        buf.append(URI.canonicalPath(url));
                    else
                        buf.append(URI.canonicalPath(URI.addPaths(URI.parentPath(_request.getRequestURI()),url)));
                    url=buf.toString();
                }
                
                super.sendRedirect(url);
            }
        }
        
        /* ------------------------------------------------------------ */
        public void setDateHeader(String name, long value)
        {
            if (!_include) super.setDateHeader(name,value);
        }
        
        /* ------------------------------------------------------------ */
        public void setHeader(String name, String value)
        {
            if (!_include) super.setHeader(name,value);
        }
        
        /* ------------------------------------------------------------ */
        public void setIntHeader(String name, int value)
        {
            if (!_include) super.setIntHeader(name,value);
        }
        
        /* ------------------------------------------------------------ */
        public void addHeader(String name, String value)
        {
            if (!_include) super.addHeader(name,value);
        }
        
        /* ------------------------------------------------------------ */
        public void addDateHeader(String name, long value)
        {
            if (!_include) super.addDateHeader(name,value);
        }
        
        /* ------------------------------------------------------------ */
        public void addIntHeader(String name, int value)
        {
            if (!_include) super.addIntHeader(name,value);
        }
        
        /* ------------------------------------------------------------ */
        public void setStatus(int status)
        {
            if (!_include) super.setStatus(status);
        }
        
        /* ------------------------------------------------------------ */
        /**
        * The default behavior of this method is to call setStatus(int sc, String sm)
        * on the wrapped response object.
        * 
        * @deprecated As of version 2.1 of the Servlet spec.
        * To set a status code 
        * use <code>setStatus(int)</code>, to send an error with a description
        * use <code>sendError(int, String)</code>.
        * 
        * @param status the status code
        * @param message the status message
        */
        public void setStatus(int status, String message)
        {
            if (!_include) super.setStatus(status,message);
        }
        
        /* ------------------------------------------------------------ */
        public void setContentLength(int len)
        {
            if (!_include) super.setContentLength(len);
        }
        
        /* ------------------------------------------------------------ */
        public void setContentType(String contentType)
        {
            if (!_include) super.setContentType(contentType);
        }
    }


    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class DontCloseWriter extends PrintWriter
    {
        DontCloseWriter(PrintWriter writer)
        {
            super(writer);
        }

        public void close()
        {}
    }

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class DontCloseServletOut extends ServletOut
    {
        DontCloseServletOut(ServletOutputStream output)
        {
            super(output);
        }

        public void close()
            throws IOException
        {}
    }
};
