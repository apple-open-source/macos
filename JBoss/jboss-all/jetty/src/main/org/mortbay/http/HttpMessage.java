// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: HttpMessage.java,v 1.15.2.9 2003/07/12 01:53:10 gregwilkins Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.StringWriter;
import java.io.Writer;
import java.util.Collections;
import java.util.Date;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import org.mortbay.util.Code;
import org.mortbay.util.QuotedStringTokenizer;
import org.mortbay.util.TypeUtil;


/* ------------------------------------------------------------ */
/** HTTP Message base.
 * This class forms the basis of HTTP requests and replies. It provides
 * header fields, content and optional trailer fields, while managing the
 * state of the message.
 *
 * @version $Id: HttpMessage.java,v 1.15.2.9 2003/07/12 01:53:10 gregwilkins Exp $
 * @author Greg Wilkins (gregw)
 */

public abstract class HttpMessage
{
    /* ------------------------------------------------------------ */
    public final static String __SCHEME ="http";
    public final static String __SSL_SCHEME ="https";
    
    /* ------------------------------------------------------------ */
    public final static String __HTTP_0_9 ="HTTP/0.9";
    public final static String __HTTP_1_0 ="HTTP/1.0";
    public final static String __HTTP_1_1 ="HTTP/1.1";
    public final static String __HTTP_1_X ="HTTP/1.";

    /* ------------------------------------------------------------ */
    public interface HeaderWriter
    {
        void writeHeader(HttpMessage httpMessage)
            throws IOException;
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /** Message States.
     */
    public final static int
        __MSG_EDITABLE=0,  // Created locally, all set methods enabled
        __MSG_BAD=1,       // Bad message/
        __MSG_RECEIVED=2,  // Received from connection.
        __MSG_SENDING=3,   // Headers sent.
        __MSG_SENT=4;      // Entity and trailers sent.

    public final static String[] __state =
    {
        "EDITABLE",
        "BAD",
        "RECEIVED",
        "SENDING",
        "SENT"
    };

    /* ------------------------------------------------------------ */
    protected int _state=__MSG_EDITABLE;
    protected String _version;
    protected int _dotVersion;
    protected HttpFields _header=new HttpFields();
    protected HttpFields _trailer;
    protected boolean _acceptTrailer;
    protected HttpConnection _connection;
    protected String _characterEncoding;
    protected String _mimeType;
    protected Object _wrapper;
    protected Map _attributes;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    protected HttpMessage()
    {}
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    protected HttpMessage(HttpConnection connection)
    {
        _connection=connection;
    }

    /* ------------------------------------------------------------ */
    /** Set a wrapper object.
     * A wrapper object is an object associated with this message and
     * presents it with an different interface. The
     * primary example of a HttpRequest facade is ServletHttpRequest.
     * A single facade object may be associated with the message with
     * this call and retrieved with the getFacade method.
     */
    public void setWrapper(Object wrapper)
    {
        _wrapper=wrapper;
    }

    /* ------------------------------------------------------------ */
    /** Get an associated wrapper object.
     * @return Wrapper message or null.
     */
    public Object getWrapper()
    {
        return _wrapper;
    }
    
    /* ------------------------------------------------------------ */
    protected void reset()
    {
        _state=__MSG_EDITABLE;
        _header=new HttpFields();
        _trailer=null;

        // XXX - also need to cancel any encodings added to output stream!
    }
    
    /* ------------------------------------------------------------ */
    public HttpConnection getHttpConnection()
    {
        return _connection;
    }

    /* ------------------------------------------------------------ */
    public InputStream getInputStream()
    {
        if (_connection==null)
            return null;
        return _connection.getInputStream();
    }
    
    /* ------------------------------------------------------------ */
    public OutputStream getOutputStream()
    {
        if (_connection==null)
            return null;
        return _connection.getOutputStream();
    }
    
    /* ------------------------------------------------------------ */
    /** Get the message state.
     * <PRE>
     * __MSG_EDITABLE = 0 - Created locally, all set methods enabled
     * __MSG_BAD      = 1 - Bad message or send failure.
     * __MSG_RECEIVED = 2 - Received from connection.
     * __MSG_SENDING  = 3 - Headers sent.
     * __MSG_SENT     = 4 - Entity and trailers sent.
     * </PRE>
     * @return the state.
     */
    public int getState()
    {
        return _state;
    }
    
    /* ------------------------------------------------------------ */
    /** Set the message state.
     * This method should be used by experts only as it can prevent
     * normal handling of a request/response.
     * @param state The new state
     * @return the last state.
     */
    public int setState(int state)
    {
        int last=_state;
        _state=state;
        return last;
    }


    /* ------------------------------------------------------------ */
    /** Get the protocol version.
     * @return return the version.
     */
    public String getVersion()
    {
        return _version;
    }
    /* ------------------------------------------------------------ */
    /** Get the protocol version.
     * @return return the version dot (0.9=-1 1.0=0 1.1=1)
     */
    public int getDotVersion()
    {
        return _dotVersion;
    }

    /* ------------------------------------------------------------ */
    /** Get field names.
     * @return Enumeration of Field Names
     */
    public Enumeration getFieldNames()
    {
        if (_header!=null && _trailer==null)
            return _header.getFieldNames();
        final Enumeration e1=_header.getFieldNames();
        final Enumeration e2=_trailer.getFieldNames();
        return new Enumeration()
            {
                public boolean hasMoreElements()
                {
                    return (e1.hasMoreElements() ||
                            e2.hasMoreElements());
                }
                   
                public Object nextElement()
                    throws NoSuchElementException
                {
                    if (e1.hasMoreElements())
                        return e1.nextElement();
                    return e2.nextElement();
                }
            };
    }

    /* ------------------------------------------------------------ */
    /** Does the header or trailer contain a field?
     * @param name Name of the field
     * @return True if contained in header or trailer.
     */
    public boolean containsField(String name)
    {
        boolean contains = _header.containsKey(name);
        if (!contains && _trailer!=null)
            contains = _trailer.containsKey(name);
        return contains;
    }
    
    /* ------------------------------------------------------------ */
    /** Get a message field.
     * Get a field from a message header. If no header field is found,
     * trailer fields are searched.
     * @param name The field name
     * @return field value or null
     */
    public String getField(String name)
    {
        String field = _header.get(name);
        if (field==null && _trailer!=null)
            field=_trailer.get(name);
        return field;
    }
    
    /* ------------------------------------------------------------ */
    /** Get a multi valued message field.
     * Get a field from a message header. If no header field is found,
     * trailer fields are searched.
     * @param name The field name
     * @return Enumeration of field values or null
     */
    public Enumeration getFieldValues(String name)
    {
        Enumeration enum = _header.getValues(name);
        if (enum==null && _trailer!=null)
            enum=_trailer.getValues(name);
        return enum;
    }
    
    /* ------------------------------------------------------------ */
    /** Get a multi valued message field.
     * Get a field from a message header. If no header field is found,
     * trailer fields are searched.
     * @param name The field name
     * @param separators String of separators.
     * @return Enumeration of field values or null
     */
    public Enumeration getFieldValues(String name,String separators)
    {
        Enumeration enum = _header.getValues(name,separators);
        if (enum==null && _trailer!=null)
            enum=_trailer.getValues(name,separators);
        return enum;
    }

    /* ------------------------------------------------------------ */
    /* Which fields to set?
     * If the message is editable, then a header fields are returned.
     * Otherwise if the message is sending a HTTP/1.1 message,
     * then a trailer field is returned if it has been set.
     * @return Header or Trailer fields
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    protected HttpFields setFields()
        throws IllegalStateException
    {
        if (_state==__MSG_EDITABLE)
            return _header;

        if (_acceptTrailer &&
            _state==__MSG_SENDING &&
            _version.equals(__HTTP_1_1))
        {
            if (_trailer==null)
                _trailer=new HttpFields();
            return _trailer;
        }
        
        throw new IllegalStateException("Can't set fields in "+
                                        __state[_state]+
                                        " for "+_version);
    }
    

    /* ------------------------------------------------------------ */
    /** Set a field value.
     * If the message is editable, then a header field is set. Otherwise
     * if the message is sending and a HTTP/1.1 version, then a trailer
     * field is set.
     * @param name Name of field 
     * @param value New value of field
     * @return Old value of field
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public String setField(String name, String value)
        throws IllegalStateException
    {
        HttpFields fields=setFields();

        if (HttpFields.__ContentType.equalsIgnoreCase(name))
            setMimeAndEncoding(value);

        return fields.put(name,value);
    }

    /* ------------------------------------------------------------ */
    /** Set the mimeType and CharacterEncodings.
     * Normally called from setField("Content-Type",type);
     * @param contentType A mimetype with optional char encoding param.
     */
    protected void setMimeAndEncoding(String contentType)
    {
        _characterEncoding=null;
        _mimeType=contentType;
        if (contentType!=null)
        {
            int i0=contentType.indexOf(';');
            if (i0>=0)
            {
                _mimeType=contentType.substring(0,i0).trim();
                int i1 = contentType.indexOf("charset=",i0);
                if (i1>=0)
                {
                    i1+=8;
                    int i2 = contentType.indexOf(' ',i1);
                    _characterEncoding = (0 < i2)
                        ? contentType.substring(i1,i2) : contentType.substring(i1);
                    _characterEncoding = QuotedStringTokenizer.unquote(_characterEncoding);
                }
            }
        }
    }
    
    
    /* ------------------------------------------------------------ */
    /** Set a multi-value field value.
     * If the message is editable, then a header field is set. Otherwise
     * if the meesage is sending and a HTTP/1.1 version, then a trailer
     * field is set.
     * @param name Name of field 
     * @param value New values of field
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void setField(String name, List value)
        throws IllegalStateException
    {
        HttpFields fields=setFields();
        fields.put(name,value);
    }
    
    /* ------------------------------------------------------------ */
    /** Add to a multi-value field value.
     * If the message is editable, then a header field is set. Otherwise
     * if the meesage is sending and a HTTP/1.1 version, then a trailer
     * field is set.
     * @param name Name of field 
     * @param value New value to add to the field
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void addField(String name, String value)
        throws IllegalStateException
    {
        HttpFields fields=setFields();
        fields.add(name,value);
    }
    
    /* -------------------------------------------------------------- */
    /** Get a field as an integer value.
     * Look in header and trailer fields.
     * Returns the value of an integer field, or -1 if not found.
     * The case of the field name is ignored.
     * @param name the case-insensitive field name
     */
    public int getIntField(String name)
    {
        int v=_header.getIntField(name);
        if (v==-1 && _trailer!=null)
            v=_trailer.getIntField(name);
        return v;
    }
    
    /* -------------------------------------------------------------- */
    /** Sets the value of an integer field.
     * Header or Trailer fields are set depending on message state.
     * @param name the field name
     * @param value the field integer value
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void setIntField(String name, int value)
        throws IllegalStateException
    {
        setFields().put(name, TypeUtil.toString(value));
    }
    
    /* -------------------------------------------------------------- */
    /** Adds the value of an integer field.
     * Header or Trailer fields are set depending on message state.
     * @param name the field name
     * @param value the field integer value
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void addIntField(String name, int value)
        throws IllegalStateException
    {
        setFields().add(name, TypeUtil.toString(value));
    }
    
    /* -------------------------------------------------------------- */
    /** Get a header as a date value.
     * Look in header and trailer fields.
     * Returns the value of a date field, or -1 if not found.
     * The case of the field name is ignored.
     * @param name the case-insensitive field name
     */
    public long getDateField(String name)
    {
        long d=_header.getDateField(name);
        if (d<0 && _trailer!=null)
            d=_trailer.getDateField(name);
        return d;
    }
    

    /* -------------------------------------------------------------- */
    /** Sets the value of a date field.
     * Header or Trailer fields are set depending on message state.
     * @param name the field name
     * @param date the field date value
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void setDateField(String name, Date date)
    {
        setFields().putDateField(name,date);
    }
    
    /* -------------------------------------------------------------- */
    /** Adds the value of a date field.
     * Header or Trailer fields are set depending on message state.
     * @param name the field name
     * @param date the field date value
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void addDateField(String name, Date date)
    {
        setFields().addDateField(name,date);
    }
    
    /* -------------------------------------------------------------- */
    /** Sets the value of a date field.
     * Header or Trailer fields are set depending on message state.
     * @param name the field name
     * @param date the field date value
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void setDateField(String name, long date)
    {
        setFields().putDateField(name,date);
    }
    
    /* -------------------------------------------------------------- */
    /** Add the value of a date field.
     * Header or Trailer fields are set depending on message state.
     * @param name the field name
     * @param date the field date value
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public void addDateField(String name, long date)
    {
        setFields().addDateField(name,date);
    }
    

    /* ------------------------------------------------------------ */
    /** Remove a field.
     * If the message is editable, then a header field is removed. Otherwise
     * if the message is sending and a HTTP/1.1 version, then a trailer
     * field is removed.
     * @param name Name of field 
     * @return Old value of field
     * @exception IllegalStateException Not editable or sending 1.1
     *                                  with trailers
     */
    public String removeField(String name)
        throws IllegalStateException
    {
        HttpFields fields=setFields();
        return fields.remove(name);
    }
    
    /* ------------------------------------------------------------ */
    /** Set the request version 
     * @param version the  HTTP version string (eg HTTP/1.1)
     * @exception IllegalStateException message is not EDITABLE
     */
    public void setVersion(String version)
    {
        if (_state!=__MSG_EDITABLE)
            throw new IllegalStateException("Not EDITABLE");
        if (version.equalsIgnoreCase(__HTTP_1_1))
        {
            _dotVersion=1;
            _version=__HTTP_1_1;
        }
        else if (version.equalsIgnoreCase(__HTTP_1_0))
        {
            _dotVersion=0;
            _version=__HTTP_1_0;
        }
        else if (version.equalsIgnoreCase(__HTTP_0_9))
        {
            _dotVersion=-1;
            _version=__HTTP_0_9;
        }
        else
            throw new IllegalArgumentException("Unknown version");
    }
    
    /* ------------------------------------------------------------ */
    /** Get the HTTP header fields.
     * @return Header or null
     */
    public HttpFields getHeader()
    {
        if (_state!=__MSG_EDITABLE)
            throw new IllegalStateException("Can't get header in "+__state[_state]);
        
        return _header;
    }
    
    /* ------------------------------------------------------------ */
    /** Get the HTTP chunked trailer (also called trailer).
     * @return Trailer or null
     */
    public HttpFields getTrailer()
    {
        if (_state!=__MSG_EDITABLE)
            throw new IllegalStateException("Can't get trailer in "+__state[_state]);
        
        if (_acceptTrailer && _trailer==null)
            _trailer=new HttpFields();
        return _trailer;
    }
    
    /* ------------------------------------------------------------ */
    /** Set if trailers are accepted.
     * @param acceptTrailer  If true, setField() may use trailers.
     */
    public void setAcceptTrailer(boolean acceptTrailer)
    {
        _acceptTrailer=acceptTrailer;
    }
    
    /* ------------------------------------------------------------ */
    /** Set if trailers are accepted.
     */
    public boolean acceptTrailer()
    {
        return _acceptTrailer;
    }
    
    /* -------------------------------------------------------------- */
    /** Character Encoding.
     * The character encoding is extracted from the ContentType field
     * when set.
     * @return Character Encoding or null
     */
    public String getCharacterEncoding()
    {
        return _characterEncoding;
    }
    
    /* ------------------------------------------------------------ */
    /** Set Character Encoding. 
     * @param encoding An encoding that can override the encoding set
     * from the ContentType field.
     */
    public void setCharacterEncoding(String encoding)
    {
        _characterEncoding=encoding;
    }
    
    /* -------------------------------------------------------------- */
    public int getContentLength()
    {
        return getIntField(HttpFields.__ContentLength);
    }
    
    /* ------------------------------------------------------------ */
    public void setContentLength(int len) 
    {
        setIntField(HttpFields.__ContentLength,len);
    }
    
    /* -------------------------------------------------------------- */
    public String getContentType()
    {
        return getField(HttpFields.__ContentType);
    }
    
    /* ------------------------------------------------------------ */
    public void setContentType(String contentType) 
    {
        setField(HttpFields.__ContentType,contentType);
    }
    
    /* -------------------------------------------------------------- */
    /** Mime Type.
     * The mime type is extracted from the contenttype field when set.
     * @return Content type without parameters
     */
    public String getMimeType()
    {
        return _mimeType;
    }
    
    /* ------------------------------------------------------------ */
    /** Recycle the message.
     */
    void recycle(HttpConnection connection)
    {
        _state=__MSG_EDITABLE;
        _version=null;
        _dotVersion=0;
        _header.clear();
        if (_trailer!=null)
            _trailer.destroy();        
        _trailer=null;
        _acceptTrailer=false;
        _connection=connection;
        _characterEncoding=null;
        _mimeType=null;
        if (_attributes!=null)
            _attributes.clear();
    }
    
    /* ------------------------------------------------------------ */
    /** Destroy the message.
     * Help the garbage collector by nulling everything that we can.
     */
    public void destroy()
    {
        recycle(null);
        if (_header!=null)
            _header.destroy();
        _header=null;
        _trailer=null;
    }
    
    /* ------------------------------------------------------------ */
    /** Convert to String.
     * The message header is converted to a String.
     * @return String
     */
    public synchronized String toString()
    {
        StringWriter writer = new StringWriter();

        int save_state=_state;
        try{
            _state=__MSG_EDITABLE;
            writeHeader(writer);
        }
        catch(IOException e)
        {
            Code.warning(e);
        }
        finally
        {
            _state=save_state;
        }
        return writer.toString();
    }


    /* ------------------------------------------------------------ */
    /** Write the message header.
     * @param writer
     */
    abstract void writeHeader(Writer writer)
        throws IOException;


    /* ------------------------------------------------------------ */
    public boolean isCommitted()
    {
        return _state==__MSG_SENDING || _state==__MSG_SENT;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return true if the message has been modified. 
     */
    public boolean isDirty()
    {
        HttpOutputStream out=(HttpOutputStream)getOutputStream();
        return _state!=__MSG_EDITABLE || ( out!=null && out.isWritten());
    }
    
    /* ------------------------------------------------------------ */
    /** Get a request attribute.
     * @param name Attribute name
     * @return Attribute value
     */
    public Object getAttribute(String name)
    {
        if (_attributes==null)
            return null;
        return _attributes.get(name);
    }

    /* ------------------------------------------------------------ */
    /** Set a request attribute.
     * @param name Attribute name
     * @param attribute Attribute value
     * @return Previous Attribute value
     */
    public Object setAttribute(String name, Object attribute)
    {
        if (_attributes==null)
            _attributes=new HashMap(11);
        return _attributes.put(name,attribute);
    }

    /* ------------------------------------------------------------ */
    /** Get Attribute names.
     * @return Enumeration of Strings
     */
    public Enumeration getAttributeNames()
    {
        if (_attributes==null)
            return Collections.enumeration(Collections.EMPTY_LIST);
        return Collections.enumeration(_attributes.keySet());
    }

    /* ------------------------------------------------------------ */
    /** Remove a request attribute.
     * @param name Attribute name
     */
    public void removeAttribute(String name)
    {
        if (_attributes!=null)
            _attributes.remove(name);
    }
}
