// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: HttpRequest.java,v 1.16.2.16 2003/07/11 00:55:12 jules_gosnell Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.io.IOException;
import java.io.InputStream;
import java.io.Writer;
import java.net.InetAddress;
import java.security.Principal;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Set;
import javax.servlet.http.Cookie;
import org.mortbay.util.ByteArrayOutputStream2;
import org.mortbay.util.Code;
import org.mortbay.util.IO;
import org.mortbay.util.InetAddrPort;
import org.mortbay.util.LazyList;
import org.mortbay.util.LineInput;
import org.mortbay.util.MultiMap;
import org.mortbay.util.QuotedStringTokenizer;
import org.mortbay.util.StringMap;
import org.mortbay.util.StringUtil;
import org.mortbay.util.TypeUtil;
import org.mortbay.util.URI;
import org.mortbay.util.UrlEncoded;


/* ------------------------------------------------------------ */
/** HTTP Request.
 * This class manages the headers, trailers and content streams
 * of a HTTP request. It can be used for receiving or generating
 * requests.
 * <P>
 * This class is not synchronized. It should be explicitly
 * synchronized if it is used by multiple threads.
 *
 * @see HttpResponse
 * @version $Id: HttpRequest.java,v 1.16.2.16 2003/07/11 00:55:12 jules_gosnell Exp $
 * @author Greg Wilkins (gregw)
 */
public class HttpRequest extends HttpMessage
{
    /* ------------------------------------------------------------ */
    /** Request METHODS.
     */
    public static final String
        __GET="GET",
        __POST="POST",
        __HEAD="HEAD",
        __PUT="PUT",
        __OPTIONS="OPTIONS",
        __DELETE="DELETE",
        __TRACE="TRACE",
        __CONNECT="CONNECT",
        __MOVE="MOVE";
    
    /* ------------------------------------------------------------ */
    /** Max size of the form content.
     * Limits the size of the data a client can push at the server.
     * Set via the org.mortbay.http.HttpRequest.maxContentSize
     * system property.
     */
    public static int __maxFormContentSize=
        Integer.getInteger("org.mortbay.http.HttpRequest.maxFormContentSize",
                           200000).intValue();
    
    /* ------------------------------------------------------------ */
    /** Maximum header line length.
     */
    public static int __maxLineLength=4096;

    
    public static final StringMap __methodCache = new StringMap(true);
    public static final StringMap __versionCache = new StringMap(true);
    static
    {
        __methodCache.put(__GET,null);
        __methodCache.put(__POST,null);
        __methodCache.put(__HEAD,null);
        __methodCache.put(__PUT,null);
        __methodCache.put(__OPTIONS,null);
        __methodCache.put(__DELETE,null);
        __methodCache.put(__TRACE,null);
        __methodCache.put(__CONNECT,null);
        __methodCache.put(__MOVE,null);

        __versionCache.put(__HTTP_1_1,null);
        __versionCache.put(__HTTP_1_0,null);
        __versionCache.put(__HTTP_0_9,null);
    }
    
    
    private static Cookie[] __noCookies = new Cookie[0];
    
    /* ------------------------------------------------------------ */
    private String _method=null;
    private URI _uri=null;
    private String _host;
    private String _hostPort;
    private int _port;
    private List _te;
    private MultiMap _parameters;
    private boolean _paramsExtracted;
    private boolean _handled;
    private Cookie[] _cookies;
    private String[] _lastCookies;
    private boolean _cookiesExtracted;
    private long _timeStamp;
    private String _timeStampStr;
    private UserPrincipal _userPrincipal;
    private String _authUser;
    private String _authType;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public HttpRequest()
    {}
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param connection 
     */
    public HttpRequest(HttpConnection connection)
    {
        super(connection);
    }

    /* ------------------------------------------------------------ */
    /** Get Request TimeStamp
     * @return The time that the request was received.
     */
    public String getTimeStampStr()
    {
        if (_timeStampStr==null && _timeStamp>0)
            _timeStampStr=HttpFields.__dateCache.format(_timeStamp);
        return _timeStampStr;
    }
    
    /* ------------------------------------------------------------ */
    /** Get Request TimeStamp
     * @return The time that the request was received.
     */
    public long getTimeStamp()
    {
        return _timeStamp;
    }
    
    /* ------------------------------------------------------------ */
    public void setTimeStamp(long ts)
    {
        _timeStamp=ts;
    }
    
    /* ------------------------------------------------------------ */
    /**
     * @deprecated use getHttpResponse()
     */
    public HttpResponse getResponse()
    {
        return getHttpResponse();
    }

    /* ------------------------------------------------------------ */
    /** Get the HTTP Response.
     * Get the HTTP Response associated with this request.
     * @return associated response
     */
    public HttpResponse getHttpResponse()
    {
        if (_connection==null)
            return null;
        return _connection.getResponse();
    }

    /* ------------------------------------------------------------ */
    /** Is the request handled.
     * @return True if the request has been set to handled or the
     * associated response is not editable.
     */
    public boolean isHandled()
    {
        if (_handled)
            return true;

        HttpResponse response= getHttpResponse();
        return (response!=null && response.getState()!=HttpMessage.__MSG_EDITABLE);
    }

    /* ------------------------------------------------------------ */
    /** Set the handled status.
     * @param handled true or false
     */
    public void setHandled(boolean handled)
    {
        _handled=handled;
    }
    
    /* ------------------------------------------------------------ */
    /** Read the request line and header.
     * @param in 
     * @exception IOException 
     */
    public void readHeader(LineInput in)
        throws IOException
    {
        _state=__MSG_BAD;
        
        // Get start line
        org.mortbay.util.LineInput.LineBuffer line_buffer;

        do
        {
            line_buffer=in.readLineBuffer();
            if (line_buffer==null)
                throw new EOFException();
        }
        while(line_buffer.size==0);
        
        if (line_buffer.size>=__maxLineLength)
            throw new HttpException(HttpResponse.__414_Request_URI_Too_Large);
        decodeRequestLine(line_buffer.buffer,line_buffer.size);
        _timeStamp=System.currentTimeMillis();
        
        // Handle version - replace with fast compare
        if (__HTTP_1_1.equals(_version))
        {
            _dotVersion=1;
            _version=__HTTP_1_1;
            _header.read(in);
            setMimeAndEncoding(_header.get(HttpFields.__ContentType));
        }
        else if (__HTTP_0_9.equals(_version))
        {
            _dotVersion=-1;
            _version=__HTTP_0_9;
        }
        else
        {
            _dotVersion=0;
            _version=__HTTP_1_0;
            _header.read(in);
            setMimeAndEncoding(_header.get(HttpFields.__ContentType));
        }

        _handled=false;
        _state=__MSG_RECEIVED;
    }
    
    /* -------------------------------------------------------------- */
    /** Write the HTTP request line as it was received.
     */
    public void writeRequestLine(Writer writer)
        throws IOException
    {
        writer.write(_method);
        writer.write(' ');
        writer.write(_uri!=null?_uri.toString():"null");
        writer.write(' ');
        writer.write(_version);
    }
    
    /* -------------------------------------------------------------- */
    /** Write the request header.
     * Places the message in __MSG_SENDING state.
     * @param writer Http output stream
     * @exception IOException IO problem
     */
    public void writeHeader(Writer writer)
        throws IOException
    {
        if (_state!=__MSG_EDITABLE)
            throw new IllegalStateException("Not MSG_EDITABLE");
        
        _state=__MSG_BAD;
        writeRequestLine(writer);
        writer.write(HttpFields.__CRLF);
        _header.write(writer);
        _state=__MSG_SENDING;
    }

    /* -------------------------------------------------------------- */
    /** Return the HTTP request line as it was received.
     */
    public String getRequestLine()
    {
        return _method+" "+_uri+" "+_version;
    }
    
    
    /* -------------------------------------------------------------- */
    /** Get the HTTP method for this request.
     * Returns the method with which the request was made. The returned
     * value can be "GET", "HEAD", "POST", or an extension method. Same
     * as the CGI variable REQUEST_METHOD.
     * @return The method
     */
    public String getMethod()
    {
        return _method;
    }

    /* ------------------------------------------------------------ */
    public void setMethod(String method)
    {
        if (getState()!=__MSG_EDITABLE)
            throw new IllegalStateException("Not EDITABLE");
        _method=method;
    }

    /* ------------------------------------------------------------ */
    public String getVersion()
    {
        return _version;
    }

    /* ------------------------------------------------------------ */
    /**
     * Reconstructs the URL the client used to make the request.
     * The returned URL contains a protocol, server name, port
     * number, and, but it does not include a path.
     * <p>
     * Because this method returns a <code>StringBuffer</code>,
     * not a string, you can modify the URL easily, for example,
     * to append path and query parameters.
     *
     * This method is useful for creating redirect messages
     * and for reporting errors.
     *
     * @return "scheme://host:port"
     */
    public StringBuffer getRootURL()
    {
        StringBuffer url = new StringBuffer (48);
        synchronized(url)
        {
            String scheme = getScheme();
            int port = getPort();

            url.append (scheme);
            url.append ("://");
            if (_hostPort!=null)
                url.append(_hostPort);
            else
            {
                url.append (getHost());
                if (port>0 && ((scheme.equalsIgnoreCase ("http") && port != 80)||
                               (scheme.equalsIgnoreCase ("https") && port != 443)))
                {
                    url.append (':');
                    url.append (port);
                }
            }
            return url;
        }
    }
    
    /* ------------------------------------------------------------ */
    /**
     * Reconstructs the URL the client used to make the request.
     * The returned URL contains a protocol, server name, port
     * number, and server path, but it does not include query
     * string parameters.
     * 
     * <p>Because this method returns a <code>StringBuffer</code>,
     * not a string, you can modify the URL easily, for example,
     * to append query parameters.
     *
     * <p>This method is useful for creating redirect messages
     * and for reporting errors.
     *
     * @return		a <code>StringBuffer</code> object containing
     *			the reconstructed URL
     *
     */
    public StringBuffer getRequestURL()
    {
        StringBuffer buf = getRootURL();
        buf.append(getPath());
        return buf;
    }
    
    /* -------------------------------------------------------------- */
    /** Get the full URI.
     * @return the request URI (not a clone).
     */
    public URI getURI()
    {
        return _uri;
    }

    
    
    /* ------------------------------------------------------------ */
    /** Get the request Scheme.
     * The scheme is obtained from an absolute URI.  If the URI in
     * the request is not absolute, then the connections default
     * scheme is returned.  If there is no connection "http" is returned.
     * @return The request scheme (eg. "http", "https", etc.)
     */
    public String getScheme()
    {
        String scheme=_uri.getScheme();
        if (scheme==null && _connection!=null)
            scheme=_connection.getDefaultScheme();
        return scheme==null?"http":scheme;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return True if this request came over an integral channel such as SSL
     */
    public boolean isIntegral()
    {
        return _connection.getListener().isIntegral(_connection);
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return True if this request came over an confidential channel such as SSL.
     */
    public boolean isConfidential()
    {
        return _connection.getListener().isConfidential(_connection);
    }
    
    /* ------------------------------------------------------------ */
    /** Get the request host.
     * @return The host name obtained from an absolute URI, the HTTP header field,
     * the requests connection or the local host name.
     */
    public String getHost()
    {
        // Return already determined host
        if (_host!=null)
            return _host;

        // Return host from absolute URI
        _host=_uri.getHost();
        _port=_uri.getPort();
        if (_host!=null)
            return _host;

        // Return host from header field
        _hostPort=_header.get(HttpFields.__Host);
        _host=_hostPort;
        _port=0;
        if (_host!=null && _host.length()>0)
        {
            int colon=_host.indexOf(':');
            if (colon>=0)
            {
                if (colon<_host.length())
                {
                    try{
                        _port=TypeUtil.parseInt(_host,colon+1,-1,10);
                    }
                    catch(Exception e)
                    {Code.ignore(e);}
                }
                _host=_host.substring(0,colon);
            }

            return _host;
        }

        // Return host from connection
        if (_connection!=null)
        {
            _host=_connection.getServerName();
            _port=_connection.getServerPort();
            if (_host!=null && !InetAddrPort.__0_0_0_0.equals(_host))
                return _host;
        }

        // Return the local host
        try {_host=InetAddress.getLocalHost().getHostAddress();}
        catch(java.net.UnknownHostException e){Code.ignore(e);}
        return _host;
    }
    
    /* ------------------------------------------------------------ */
    /** Get the request port.
     * The port is obtained either from an absolute URI, the HTTP
     * Host header field, the connection or the default.
     * @return The port.  0 should be interpreted as the default port.
     */
    public int getPort()
    {
        if (_port>0)
            return _port;
        if (_host!=null)
            return 0;
        if (_uri.isAbsolute())
            _port=_uri.getPort();
        else if (_connection!=null)
            _port=_connection.getServerPort();
        return _port;    
    }
    
    /* ------------------------------------------------------------ */
    /** Get the request path.
     * @return The URI path of the request.
     */
    public String getPath()
    {
        return _uri.getPath();
    }

    /* ------------------------------------------------------------ */
    public void setPath(String path)
    {
        if (getState()!=__MSG_EDITABLE)
            throw new IllegalStateException("Not EDITABLE");
        if (_uri==null)
            _uri=new URI(path);
        else
            _uri.setURI(path);
    }
    
    /* ------------------------------------------------------------ */
    /** Get the encoded request path.
     * @return The path with % encoding.
     */
    public String getEncodedPath()
    {
        return _uri.getEncodedPath();
    }
    
    /* ------------------------------------------------------------ */
    /** Get the request query.
     * @return the request query excluding the '?'
     */
    public String getQuery()
    {
        return _uri.getQuery();
    }
    
    /* ------------------------------------------------------------ */
    public void setQuery(String q)
    {   
        if (getState()!=__MSG_EDITABLE)
            throw new IllegalStateException("Not EDITABLE");
        _uri.setQuery(q);
    }
    
    /* ------------------------------------------------------------ */
    public String getRemoteAddr()
    {
        String addr="127.0.0.1";
        HttpConnection connection = getHttpConnection();
        if (connection!=null)
        {
            addr=connection.getRemoteAddr();
            if (addr==null)
                addr=connection.getRemoteHost();
        }
        return addr;
    }
    
    /* ------------------------------------------------------------ */
    public String getRemoteHost()
    {
        String host="127.0.0.1";
        HttpConnection connection = getHttpConnection();
        if (connection!=null)
        {
            host=connection.getRemoteHost();
            if (host==null)
                host=connection.getRemoteAddr();
        }
        return host;
    }
    
    /* ------------------------------------------------------------ */
    /** Decode HTTP request line.
     * @param buf Character buffer
     * @param len Length of line in buffer.
     * @exception IOException 
     */
    void decodeRequestLine(char[] buf,int len)
        throws IOException
    {        
        // Search for first space separated chunk
        int s1=-1,s2=-1,s3=-1;
        int state=0;
    startloop:
        for (int i=0;i<len;i++)
        {
            char c=buf[i];
            switch(state)
            {
              case 0: // leading white
                  if (c==' ')
                      continue;
                  state=1;
                  s1=i;
                  
              case 1: // reading method
                  if (c==' ')
                      state=2;
                  else
                  {
                      s2=i;
                      if (c>='a'&&c<='z')
                          buf[i]=(char)(c-'a'+'A');
                  }
                  continue;
                  
              case 2: // skip whitespace after method
                  s3=i;
                  if (c!=' ')
                      break startloop;
            }
        }

        // Search for last space separated chunk
        int e1=-1,e2=-1,e3=-1;
        state=0;
    endloop:
        for (int i=len;i-->0;)
        {
            char c=buf[i];
            switch(state)
            {
              case 0: // trailing white
                  if (c==' ')
                      continue;
                  state=1;
                  e1=i;
                  
              case 1: // reading Version
                  if (c==' ')
                      state=2;
                  else
                      e2=i;
                  continue;
                  
              case 2: // skip whitespace before version
                  e3=i;
                  if (c!=' ')
                      break endloop;
            }
        }
        
        // Check sufficient params
        if (s3<0 || e1<0 || e3<s2 )
            throw new IOException("Bad Request: "+new String(buf,0,len));

        // get method
        Map.Entry method = __methodCache.getEntry(buf,s1,s2-s1+1);
        if (method!=null)
            _method=(String)method.getKey();
        else
            _method=new String(buf,s1,s2-s1+1).toUpperCase();
        
        // get version as uppercase
        if (s2!=e3 || s3!=e2)
        {
            Map.Entry version = __versionCache.getEntry(buf,e2,e1-e2+1);
            if (version!=null)
                _version=(String)version.getKey();
            else
            {
                for (int i=e2;i<=e1;i++)
                    if (buf[i]>='a'&&buf[i]<='z')
                        buf[i]=(char)(buf[i]-'a'+'A');
                _version=new String(buf,e2,e1-e2+1);
            }
        }
        else
        {
            // missing version
            _version=__HTTP_0_9;
            e3=e1;
        }

        // handle URI
        try{
            String raw_uri=new String(buf,s3,e3-s3+1);
            if (_uri==null)
                _uri= new URI(raw_uri);
            else
                _uri.setURI(raw_uri);
        }
        catch(IllegalArgumentException e)
        {
            Code.ignore(e);
            throw new HttpException(HttpResponse.__400_Bad_Request,new String(buf,s3,e3-s3+1));
        }            
    }
    
    /* ------------------------------------------------------------ */
    /** Force a removeField.
     * This call ignores the message state and forces a field
     * to be removed from the request.  It is required for the
     * handling of the Connection field.
     * @param name The field name
     * @return The old value or null.
     */
    Object forceRemoveField(String name)
    {
        int saved_state=_state;
        try{
            _state=__MSG_EDITABLE;
            return removeField(name);
        }
        finally
        {
            _state=saved_state;
        }
    }


    /* ------------------------------------------------------------ */
    /** Get the acceptable transfer encodings.
     * The TE field is used to construct a list of acceptable
     * extension transfer codings in quality order.
     * An empty list implies that only "chunked" is acceptable.
     * A null list implies that no transfer coding can be applied.
     *
     * If the "trailer" coding is found in the TE field, then
     * message trailers are enabled in any linked response.
     * @return List of codings.
     */
    public List getAcceptableTransferCodings()
    {
        if (_dotVersion<1)
            return null;
        if (_te!=null)
            return _te;
        
        // Decode any TE field
        Enumeration tenum = getFieldValues(HttpFields.__TE,
                                           HttpFields.__separators);
        
        if (tenum!=null)
        {
            // Sort the list
            List te=HttpFields.qualityList(tenum);
            int size=te.size();
            // Process if something there
            if (size >0)
            {
                Object acceptable = null;
                
                // remove trailer and chunked items.
                ListIterator iter = te.listIterator();
                while(iter.hasNext())
                {
                    String coding= StringUtil.asciiToLowerCase
                        (HttpFields.valueParameters(iter.next().toString(),null));
                    
                    if ("trailer".equalsIgnoreCase(coding))
                    {
                        // Allow trailers in the response
                        HttpResponse response=getHttpResponse();
                        if (response!=null)
                            response.setAcceptTrailer(true);
                    }
                    else if (!HttpFields.__Chunked.equalsIgnoreCase(coding))
                        acceptable=LazyList.add(acceptable,size,coding);
                }
                _te=LazyList.getList(acceptable);
            }
            else
                _te=Collections.EMPTY_LIST;
        }
        else
            _te=Collections.EMPTY_LIST;

        return _te;
    }


    /* ------------------------------------------------------------ */
    /* Extract Paramters from query string and/or form content.
     */
    private void extractParameters()
    {        
        if (_paramsExtracted)
            return;
        _paramsExtracted=true;

        if (_parameters==null)
            _parameters=new MultiMap(16);

        // Handle query string
        String encoding=getCharacterEncoding();
        if (encoding==null)
        {
            // No encoding, so use the existing characters.
            encoding=StringUtil.__ISO_8859_1;
            _uri.putParametersTo(_parameters);
        }
        else
        {
            // An encoding has been set, so reencode query string.
            String query=_uri.getQuery();
            if (query!=null)
            {
                try
                {
                    byte[]qbytes=query.getBytes(StringUtil.__ISO_8859_1);
                    UrlEncoded.decodeTo(qbytes,_parameters,encoding);
                }
                catch(Exception e)
                {
                    _uri.putParametersTo(_parameters);
                }
            }
        }

        // handle any content.
        if (_state==__MSG_RECEIVED)
        {
            String content_type=getField(HttpFields.__ContentType);
            if (content_type!=null && content_type.length()>0)
            {
                content_type=StringUtil.asciiToLowerCase(content_type);
                content_type=HttpFields.valueParameters(content_type,null);

                if (HttpFields.__WwwFormUrlEncode.equalsIgnoreCase(content_type)&&
                    HttpRequest.__POST.equals(getMethod()))
                {
                    int content_length = getIntField(HttpFields.__ContentLength);
                    if (content_length==0)
                        Code.debug("No form content");
                    else
                    {
                        try
                        {
                            // Read the content
                            ByteArrayOutputStream2 bout =
                                new ByteArrayOutputStream2(content_length>0?content_length:4096);
                            InputStream in = getInputStream();
                            int max=content_length;
                            if (max<0)
                                max=__maxFormContentSize;
                            else if (max>__maxFormContentSize)
                            {
                                Code.warning("Form content truncated");
                                max=__maxFormContentSize;
                            }

                            // Copy to a byte array.
                            // XXX - this is very inefficient and we could
                            // save lots of memory by streaming this!!!!
                            IO.copy(in,bout,max);
                            
                            byte[] content=bout.getBuf();

                            // Add form params to query params
                            UrlEncoded.decodeTo(content,_parameters,encoding);
                        }
                        catch (EOFException e)
                        {
                            Code.ignore(e);
                        }
                        catch (IOException e)
                        {
                            if (Code.debug())
                                Code.warning(e);
                            else
                                Code.warning(e.toString());
                        }
                    }
                }
            }
        }
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return Map of parameters
     */
    public MultiMap getParameters()
    {
        if (!_paramsExtracted)
            extractParameters();
        return _parameters;
    }
    
    
    /* ------------------------------------------------------------ */
    /** Get the set of parameter names.
     * @return Set of parameter names.
     */
    public Set getParameterNames()
    {
        if (!_paramsExtracted)
            extractParameters();
        return _parameters.keySet();
    }
    
    /* ------------------------------------------------------------ */
    /** Get a parameter value.
     * @param name Parameter name
     * @return Parameter value
     */
    public String getParameter(String name)
    {
        if (!_paramsExtracted)
            extractParameters();
        return (String)_parameters.getValue(name,0);
    }
    
    /* ------------------------------------------------------------ */
    /** Get multi valued paramater.
     * @param name Parameter name
     * @return Parameter values
     */
    public List getParameterValues(String name)
    {
        if (!_paramsExtracted)
            extractParameters();
        return _parameters.getValues(name);
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return Parameters as a map of String arrays
     */
    public Map getParameterStringArrayMap()
    {
        if (!_paramsExtracted)
            extractParameters();
        return _parameters.toStringArrayMap();
    }

    /* -------------------------------------------------------------- */
    /** Extract received cookies from a header.
     * @return Array of Cookies.
     */
    public Cookie[] getCookies()
    {
        if (_cookies!=null && _cookiesExtracted)
            return _cookies;

        try
        {
            // Handle no cookies
            if(!_header.containsKey(HttpFields.__Cookie))
            {
                _cookies=__noCookies;
                _cookiesExtracted=true;
                _lastCookies=null;
                return _cookies;
            }

            // Check if cookie headers match last cookies
            if (_lastCookies!=null)
            {                
                int last=0;
                Enumeration enum =_header.getValues(HttpFields.__Cookie);
                while (enum.hasMoreElements())
                {
                    String c = (String)enum.nextElement();
                    if (last>=_lastCookies.length ||
                        !c.equals(_lastCookies[last]))
                    {
                        _lastCookies=null;
                        break;
                    }
                    last++;
                }
                if (_lastCookies!=null)
                {
                    _cookiesExtracted=true;
                    return _cookies;
                }    
            }
            
            // Get ready to parse cookies (Expensive!!!)
            Object cookies=null;
            Object lastCookies=null;
            
            int version=0;
            Cookie cookie=null;

            // For each cookie header
            Enumeration enum =_header.getValues(HttpFields.__Cookie);            
            while (enum.hasMoreElements())
            {
                // Save a copy of the unparsed header as cache.
                String hdr = enum.nextElement().toString();
                lastCookies=LazyList.add(lastCookies,hdr);

                // Parse the header
                QuotedStringTokenizer tok=new QuotedStringTokenizer(hdr,",;",false,false);
                while (tok.hasMoreElements())
                {
                    String c=(String)tok.nextElement();
                    if (c!=null)
                        c=c.trim();
                    
                    try
                    {                   
                        String n;
                        String v;
                        if (c.startsWith("JSESSION_ID="))
                        {
                            n="JSESSION_ID";
                            v=c.substring(12);    
                        }
                        else
                        {
                            int e = c.indexOf('=');
                            if (e>0)
                            {
                                n=c.substring(0,e);
                                v=c.substring(e+1);
                            }
                            else
                            {
                                n=c;
                                v="";
                            }
                        }
                        
                        // Handle quoted values
                        if (version>0)
                            v=StringUtil.unquote(v);
                        
                        // Ignore $ names
                        if (n.startsWith("$"))
                        {
                            if ("$version".equalsIgnoreCase(n))
                            {
                                int comma=v.indexOf(',');
                                if (comma>=0)
                                {   
                                    version=Integer.parseInt
                                        (StringUtil.unquote(v.substring(0,comma)));
                                    v=v.substring(comma+1);
                                    int e=v.indexOf('=');
                                    if (e>0)
                                    {
                                        n=v.substring(0,e);
                                        v=v.substring(e+1);
                                        v=StringUtil.unquote(v);
                                    }
                                    else
                                    {
                                        n=v;
                                        v="";
                                    }
                                }
                                else
                                    continue;
                            }
                            else
                            {
                                if ("$path".equalsIgnoreCase(n) && cookie!=null)
                                    cookie.setPath(v);
                                else if ("$domain".equalsIgnoreCase(n)&&cookie!=null)
                                    cookie.setDomain(v);
                                continue;
                            }
                        }
                        
                        v=URI.decodePath(v);
                        cookie=new Cookie(n,v);
                        if (version>0)
                            cookie.setVersion(version);
                        cookies=LazyList.add(cookies,cookie);
                    }
                    catch(Exception e)
                    {
                        Code.ignore(e);
                        Code.warning("Bad Cookie received: "+e.toString());
                    }
                }
            }

            int l=LazyList.size(cookies);
            if (_cookies==null || _cookies.length!=l)
                _cookies=new Cookie[l];
            for (int i=0;i<l;i++)
                _cookies[i]=(Cookie)LazyList.get(cookies,i);
            _cookiesExtracted=true;
            
            l=LazyList.size(lastCookies);
            _lastCookies=new String[l];
            for (int i=0;i<l;i++)
                _lastCookies[i]=(String)LazyList.get(lastCookies,i);
            
        }
        catch(Exception e)
        {
            Code.warning(e);
        }
        
        return _cookies;
    }


    /* ------------------------------------------------------------ */
    public boolean isUserInRole(String role)
    {
        Principal principal=getUserPrincipal();
        if (principal!=null && principal instanceof UserPrincipal)
            return ((UserPrincipal)principal).isUserInRole(role);
        return false;
    }
    
    /* ------------------------------------------------------------ */
    public String getAuthType()
    {
        return _authType;
    }
    
    /* ------------------------------------------------------------ */
    public void setAuthType(String a)
    {
        _authType=a;
    }
    
    /* ------------------------------------------------------------ */
    public String getAuthUser()
    {
        return _authUser;
    }
    
    /* ------------------------------------------------------------ */
    public void setAuthUser(String user)
    {
        _authUser=user;
    }
    
    /* ------------------------------------------------------------ */
    public UserPrincipal getUserPrincipal()
    {
        return _userPrincipal;
    }
    
    /* ------------------------------------------------------------ */
    public void setUserPrincipal(UserPrincipal principal)
    {
        _userPrincipal=principal;
    }
    
    /* ------------------------------------------------------------ */
    /** Recycle the request.
     */
    void recycle(HttpConnection connection)
    {
        _method=null;
        //_uri=null;
        _host=null;
        _hostPort=null;
        _port=0;
        _te=null;
        if (_parameters!=null)
            _parameters.clear();
        _paramsExtracted=false;
        _handled=false;
        _cookiesExtracted=false;
        _timeStamp=0;
        _timeStampStr=null;
        _authUser=null;
        _authType=null;
        _userPrincipal=null;
        super.recycle(connection);
    }
    
    /* ------------------------------------------------------------ */
    /** Destroy the request.
     * Help the garbage collector by null everything that we can.
     */
    public void destroy()
    {
        _parameters=null;
        _method=null;
        _uri=null;
        _host=null;
        _hostPort=null;
        _te=null;
        _cookies=null;
        _lastCookies=null;
        _timeStampStr=null;
        _userPrincipal=null;
        _authUser=null;
        _authUser=null;
        if (_attributes!=null)
            _attributes.clear();
        super.destroy();
    }
}
