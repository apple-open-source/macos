// ========================================================================
// Copyright (c) 2000 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ServletHttpResponse.java,v 1.15.2.12 2003/06/04 04:47:52 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import javax.servlet.ServletOutputStream;
import javax.servlet.ServletResponse;
import javax.servlet.ServletResponseWrapper;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import org.mortbay.http.HttpFields;
import org.mortbay.http.HttpOutputStream;
import org.mortbay.http.HttpResponse;
import org.mortbay.util.Code;
import org.mortbay.util.IO;
import org.mortbay.util.StringUtil;
import org.mortbay.util.URI;

/* ------------------------------------------------------------ */
/** Servlet Response Wrapper.
 * This class wraps a Jetty HTTP response as a 2.2 Servlet
 * response.
 *
 * Note that this wrapper is not synchronized and if a response is to
 * be operated on by multiple threads, then higher level
 * synchronizations may be required.
 *
 * @version $Id: ServletHttpResponse.java,v 1.15.2.12 2003/06/04 04:47:52 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class ServletHttpResponse implements HttpServletResponse
{
    public static final int
        DISABLED=-1,
        NO_OUT=0,
        OUTPUTSTREAM_OUT=1,
        WRITER_OUT=2;

    private static ServletWriter __nullServletWriter;
    private static ServletOut __nullServletOut;
    static
    {
        try{
            __nullServletWriter =
                new ServletWriter(IO.getNullStream());
            __nullServletOut =
                new ServletOut(IO.getNullStream());
        }
        catch (Exception e)
        {
            Code.fail(e);
        }
    }
    
    private static Map __charSetMap = new HashMap();
    static
    {
        // list borrowed from tomcat 3.2B6 - thanks guys.
        __charSetMap.put("ar", "ISO-8859-6");
        __charSetMap.put("be", "ISO-8859-5");
        __charSetMap.put("bg", "ISO-8859-5");
        __charSetMap.put("ca", StringUtil.__ISO_8859_1);
        __charSetMap.put("cs", "ISO-8859-2");
        __charSetMap.put("da", StringUtil.__ISO_8859_1);
        __charSetMap.put("de", StringUtil.__ISO_8859_1);
        __charSetMap.put("el", "ISO-8859-7");
        __charSetMap.put("en", StringUtil.__ISO_8859_1);
        __charSetMap.put("es", StringUtil.__ISO_8859_1);
        __charSetMap.put("et", StringUtil.__ISO_8859_1);
        __charSetMap.put("fi", StringUtil.__ISO_8859_1);
        __charSetMap.put("fr", StringUtil.__ISO_8859_1);
        __charSetMap.put("hr", "ISO-8859-2");
        __charSetMap.put("hu", "ISO-8859-2");
        __charSetMap.put("is", StringUtil.__ISO_8859_1);
        __charSetMap.put("it", StringUtil.__ISO_8859_1);
        __charSetMap.put("iw", "ISO-8859-8");
        __charSetMap.put("ja", "Shift_JIS");
        __charSetMap.put("ko", "EUC-KR");     
        __charSetMap.put("lt", "ISO-8859-2");
        __charSetMap.put("lv", "ISO-8859-2");
        __charSetMap.put("mk", "ISO-8859-5");
        __charSetMap.put("nl", StringUtil.__ISO_8859_1);
        __charSetMap.put("no", StringUtil.__ISO_8859_1);
        __charSetMap.put("pl", "ISO-8859-2");
        __charSetMap.put("pt", StringUtil.__ISO_8859_1);
        __charSetMap.put("ro", "ISO-8859-2");
        __charSetMap.put("ru", "ISO-8859-5");
        __charSetMap.put("sh", "ISO-8859-5");
        __charSetMap.put("sk", "ISO-8859-2");
        __charSetMap.put("sl", "ISO-8859-2");
        __charSetMap.put("sq", "ISO-8859-2");
        __charSetMap.put("sr", "ISO-8859-5");
        __charSetMap.put("sv", StringUtil.__ISO_8859_1);
        __charSetMap.put("tr", "ISO-8859-9");
        __charSetMap.put("uk", "ISO-8859-5");
        __charSetMap.put("zh", "GB2312");
        __charSetMap.put("zh_TW", "Big5");    
    }
    
    /* ------------------------------------------------------------ */
    private HttpResponse _httpResponse;
    private ServletHttpRequest _servletHttpRequest;
    private int _outputState=NO_OUT;
    private ServletOut _out =null;
    private ServletWriter _writer=null;
    private HttpSession _session=null;
    private boolean _noSession=false;
    private Locale _locale=null;

    
    /* ------------------------------------------------------------ */
    ServletHttpResponse(ServletHttpRequest request,HttpResponse response)
    {
        _servletHttpRequest=request;
        _servletHttpRequest.setServletHttpResponse(this);
        _httpResponse=response;
    }

    /* ------------------------------------------------------------ */
    void recycle()
    {
        _outputState=NO_OUT;
        _out=null;
        _writer=null;
        _session=null;
        _noSession=false;
        _locale=null;
    }
    
    /* ------------------------------------------------------------ */
    int getOutputState()
    {
        return _outputState;
    }
    
    /* ------------------------------------------------------------ */
    void setOutputState(int s)
        throws IOException
    {
        if (s<0)
        {
            _outputState=DISABLED;
            if (_writer!=null)
                _writer.disable();
            _writer=null;
            if (_out!=null)
                _out.disable();
            _out=null;
        }
        else
            _outputState=s;
    }
    
    
    /* ------------------------------------------------------------ */
    HttpResponse getHttpResponse()
    {
        return _httpResponse;
    }
    
    /* ------------------------------------------------------------ */
    void commit()
        throws IOException
    {
        _httpResponse.commit();
    }

    /* ------------------------------------------------------------ */
    public boolean isCommitted()
    {
        return _httpResponse.isCommitted();
    }
    
    /* ------------------------------------------------------------ */
    boolean isDirty()
    {
        return _httpResponse.isDirty();
    }

    /* ------------------------------------------------------------ */
    public void setBufferSize(int size)
    {
        HttpOutputStream out = (HttpOutputStream)_httpResponse.getOutputStream();
        if (out.isWritten()  || _writer!=null && _writer.isWritten())
            throw new IllegalStateException("Output written");
        out.setBufferSize(size);
    }
    
    /* ------------------------------------------------------------ */
    public int getBufferSize()
    {
        return ((HttpOutputStream)_httpResponse.getOutputStream()).getBufferSize();
    }
    
    /* ------------------------------------------------------------ */
    public void flushBuffer()
        throws IOException
    {
        if (((HttpOutputStream)_httpResponse.getOutputStream()).isClosed())
            return;
        
        if (_writer!=null)
            _writer.flush();
        if (_out!=null)
            _out.flush();
        if (_writer==null && _out==null)
            _httpResponse.getOutputStream().flush();
        if (!_httpResponse.isCommitted())
            _httpResponse.commit();
    }
    
    /* ------------------------------------------------------------ */
    public void resetBuffer()
    {
        if (isCommitted())
            throw new IllegalStateException("committed");
        ((HttpOutputStream)_httpResponse.getOutputStream()).resetBuffer();
        if (_writer!=null)
            _writer.reset();
    }
    
    /* ------------------------------------------------------------ */
    public void reset()
    {
        resetBuffer();
        _httpResponse.reset();
    }
    
    /* ------------------------------------------------------------ */
    /**
     * Sets the locale of the response, setting the headers (including the
     * Content-Type's charset) as appropriate.  This method should be called
     * before a call to {@link #getWriter}.  By default, the response locale
     * is the default locale for the server.
     * 
     * @see   #getLocale
     * @param locale the Locale of the response
     */
    public void setLocale(Locale locale)
    {
        if (locale == null)
            return; 

        _locale = locale;
        setHeader(HttpFields.__ContentLanguage,locale.toString().replace('_','-'));
                          
        /* get current MIME type from Content-Type header */                  
        String type=_httpResponse.getField(HttpFields.__ContentType);
        if (type==null)
        {
            // servlet did not set Content-Type yet
            // so lets assume default one
            type="application/octet-stream";
        }
        else if (type.startsWith("text/") && _httpResponse.getCharacterEncoding()==null)
            /* If there is already charset parameter in content-type,
               we will leave it alone as it is already correct.
               This allows for both setContentType() and setLocale() to be called.
               It makes some sense for text/ MIME types to try to guess output encoding 
               based on language code from locale when charset parameter is not present.
               Guessing is not a problem because when encoding matters, setContentType()
               should be called with charset parameter in MIME type.
              
               JH: I think guessing should be exterminated as it makes no sense.
            */
        {
            /* pick up encoding from map based on languge code */
            String lang = locale.getLanguage();
            String charset = (String)__charSetMap.get(lang);
            if (charset != null && charset.length()>0)
            {
                int semi=type.indexOf(';');
                if (semi<0)
                    type += "; charset="+charset;
                else
                    type = type.substring(0,semi)+"; charset="+charset;
            }
        }        
        /* lets put updated MIME type back */
        setHeader(HttpFields.__ContentType,type);
    }
    
    /* ------------------------------------------------------------ */
    public Locale getLocale()
    {
        return _locale;
    }
    
    /* ------------------------------------------------------------ */
    public void addCookie(Cookie cookie) 
    {
        _httpResponse.addSetCookie(cookie);
    }

    /* ------------------------------------------------------------ */
    public boolean containsHeader(String name) 
    {
        return _httpResponse.containsField(name);
    }

    /* ------------------------------------------------------------ */
    public String encodeURL(String url) 
    {        
        // should not encode if cookies in evidence
        if (_servletHttpRequest==null ||
            _servletHttpRequest.isRequestedSessionIdFromCookie() &&
            _servletHttpRequest.getServletHandler().isUsingCookies())
            return url;        
        
        // get session;
        if (_session==null && !_noSession)
        {
            _session=_servletHttpRequest.getSession(false);
            _noSession=(_session==null);
        }
        
        // no session or no url
        if (_session == null || url==null)
            return url;
        
        // invalid session
        String id = _session.getId();
        if (id == null)
            return url;
        
        // Check host and port are for this server
        // XXX not implemented
        
        // Already encoded
        int prefix=url.indexOf(SessionManager.__SessionUrlPrefix);
        if (prefix!=-1)
        {
            int suffix=url.indexOf("?",prefix);
            if (suffix<0)
                suffix=url.indexOf("#",prefix);

            if (suffix<=prefix)
                return url.substring(0,prefix+SessionManager.__SessionUrlPrefix.length())+id;
            return url.substring(0,prefix+SessionManager.__SessionUrlPrefix.length())+id+
                url.substring(suffix);
        }        
        
        // edit the session
        int suffix=url.indexOf('?');
        if (suffix<0)
            suffix=url.indexOf('#');
        if (suffix<0)
            return url+SessionManager.__SessionUrlPrefix+id;
        return url.substring(0,suffix)+
            SessionManager.__SessionUrlPrefix+id+url.substring(suffix);
    }

    /* ------------------------------------------------------------ */
    public String encodeRedirectURL(String url) 
    {
        return encodeURL(url);
    }

    /* ------------------------------------------------------------ */
    /**
     * @deprecated	As of version 2.1, use encodeURL(String url) instead
     */
    public String encodeUrl(String url) 
    {
        return encodeURL(url);
    }

    /* ------------------------------------------------------------ */
    /**
     * @deprecated	As of version 2.1, use 
     *			encodeRedirectURL(String url) instead
     */
    public String encodeRedirectUrl(String url) 
    {
        return encodeRedirectURL(url);
    }

    /* ------------------------------------------------------------ */
    public void sendError(int status, String message)
        throws IOException
    {
        ServletHolder holder = _servletHttpRequest.getServletHolder();
        if (holder!=null)
            _servletHttpRequest.setAttribute("javax.servlet.error.servlet_name",
                                             holder.getName());
        _httpResponse.sendError(status,message);
    }

    /* ------------------------------------------------------------ */
    public void sendError(int status) 
        throws IOException
    {
        ServletHolder holder = _servletHttpRequest.getServletHolder();
        if (holder!=null)
            _servletHttpRequest.setAttribute("javax.servlet.error.servlet_name",
                                             holder.getName());
        _httpResponse.sendError(status);
    }

    /* ------------------------------------------------------------ */
    public void sendRedirect(String url) 
        throws IOException
    {
        if (url==null)
            throw new IllegalArgumentException();
        
        if (!URI.hasScheme(url))
        {
            StringBuffer buf = _servletHttpRequest.getHttpRequest().getRootURL();
            if (url.startsWith("/"))
                buf.append(URI.canonicalPath(url));
            else
            {
                String path=_servletHttpRequest.getRequestURI();
                String parent=(path.endsWith("/"))?path:URI.parentPath(path);
                url=URI.canonicalPath(URI.addPaths(parent,url));
                if (!url.startsWith("/"))
                    buf.append('/');
                buf.append(url);
            }
            
            url=buf.toString();
        }
        _httpResponse.sendRedirect(url);
    }

    /* ------------------------------------------------------------ */
    public void setDateHeader(String name, long value) 
    {
        try{_httpResponse.setDateField(name,value);}
        catch(IllegalStateException e){Code.ignore(e);}
    }

    /* ------------------------------------------------------------ */
    public void setHeader(String name, String value) 
    {
        try{_httpResponse.setField(name,value);}
        catch(IllegalStateException e){Code.ignore(e);}
    }

    /* ------------------------------------------------------------ */
    public void setIntHeader(String name, int value) 
    {
        try{_httpResponse.setIntField(name,value);}
        catch(IllegalStateException e){Code.ignore(e);}
    }
    
    /* ------------------------------------------------------------ */
    public void addDateHeader(String name, long value) 
    {
        try{_httpResponse.addDateField(name,value);}
        catch(IllegalStateException e){Code.ignore(e);}
    }

    /* ------------------------------------------------------------ */
    public void addHeader(String name, String value) 
    {
        try{_httpResponse.addField(name,value);}
        catch(IllegalStateException e){Code.ignore(e);}
    }
    

    /* ------------------------------------------------------------ */
    public void addIntHeader(String name, int value) 
    {
        try{_httpResponse.addIntField(name,value);}
        catch(IllegalStateException e){Code.ignore(e);}
    }

    /* ------------------------------------------------------------ */
    public void setStatus(int status) 
    {
        _httpResponse.setStatus(status);
    }

    /* ------------------------------------------------------------ */
    /**
    * @deprecated As of version 2.1 of the Servlet spec.
    * To set a status code 
    * use <code>setStatus(int)</code>, to send an error with a description
    * use <code>sendError(int, String)</code>.
    *
    * Sets the status code and message for this response.
    * 
    * @param status the status code
    * @param message the status message
    */
    public void setStatus(int status, String message) 
    {
        setStatus(status);
        _httpResponse.setReason(message);
    }

    /* ------------------------------------------------------------ */
    public String getCharacterEncoding() 
    {
        String encoding=_httpResponse.getCharacterEncoding();
        return (encoding==null)?StringUtil.__ISO_8859_1:encoding;
    }

    /* ------------------------------------------------------------ */
    public ServletOutputStream getOutputStream() 
    {
        if (_outputState==DISABLED)
            return __nullServletOut;
        
        if (_outputState!=NO_OUT && _outputState!=OUTPUTSTREAM_OUT)
            throw new IllegalStateException();
        
        if (_writer!=null)
        {
            _writer.flush();
            _writer.disable();
            _writer=null;
        }
        
        if (_out==null)
            _out = new ServletOut(_servletHttpRequest.getHttpRequest()
                                  .getOutputStream());  
        _outputState=OUTPUTSTREAM_OUT;
        return _out;
    }

    /* ------------------------------------------------------------ */
    public PrintWriter getWriter()
        throws java.io.IOException 
    {
        if (_outputState==DISABLED)
            return __nullServletWriter;
                                   
        if (_outputState!=NO_OUT && _outputState!=WRITER_OUT)
            throw new IllegalStateException();

        // If we are switching modes, flush output to try avoid overlaps.
        if (_out!=null)
            _out.flush();
        
        /* if there is no writer yet */
        if (_writer==null)
        {
            /* get encoding from Content-Type header */
            String encoding = getCharacterEncoding();
            if (encoding==null && _servletHttpRequest!=null)
            {
                /* implementation of educated defaults */
                String mimeType = _httpResponse.getMimeType();                
                encoding = _servletHttpRequest.getServletHandler()
                    .getHttpContext().getEncodingByMimeType(mimeType);
            }
            if (encoding==null)
                // get last resort hardcoded default
                encoding = StringUtil.__ISO_8859_1; 
            
            /* construct Writer using correct encoding */
            _writer = new ServletWriter(_httpResponse.getOutputStream(), encoding);
        }                    
        _outputState=WRITER_OUT;
        return _writer;
    }
    
    /* ------------------------------------------------------------ */
    public void setContentLength(int len) 
    {
        // Protect from setting after committed as default handling
        // of a servlet HEAD request ALWAYS sets content length, even
        // if the getHandling committed the response!
        if (!isCommitted())
            setIntHeader(HttpFields.__ContentLength,len);
    }
    
    /* ------------------------------------------------------------ */
    public void setContentType(String contentType) 
    {
        setHeader(HttpFields.__ContentType,contentType);
        if (_locale!=null)
            setLocale(_locale);
    }

    /* ------------------------------------------------------------ */
    public String toString()
    {
        return _httpResponse.toString();
    }

    
    /* ------------------------------------------------------------ */
    /** Unwrap a ServletResponse.
     *
     * @see javax.servlet.ServletResponseWrapper
     * @see javax.servlet.http.HttpServletResponseWrapper
     * @param response 
     * @return The core ServletHttpResponse which must be the
     * underlying response object 
     */
    public static ServletHttpResponse unwrap(ServletResponse response)
    {
        while (!(response instanceof ServletHttpResponse))
        {
            if (response instanceof ServletResponseWrapper)
            {
                ServletResponseWrapper wrapper =
                    (ServletResponseWrapper)response;
                response=wrapper.getResponse();
            }
            else
                throw new IllegalArgumentException("Does not wrap ServletHttpResponse");
        }

        return (ServletHttpResponse)response;
    }
    
}





