// ========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: MultiPartRequest.java,v 1.15.2.6 2003/06/04 04:47:56 starksm Exp $
// ------------------------------------------------------------------------

package org.mortbay.servlet;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Hashtable;
import java.util.StringTokenizer;
import javax.servlet.http.HttpServletRequest;
import org.mortbay.http.HttpFields;
import org.mortbay.util.Code;
import org.mortbay.util.LineInput;
import org.mortbay.util.StringUtil;

/* ------------------------------------------------------------ */
/** Multipart Form Data request.
 * <p>
 * This class decodes the multipart/form-data stream sent by
 * a HTML form that uses a file input item.
 *
 * <p><h4>Usage</h4>
 * Each part of the form data is named from the HTML form and
 * is available either via getString(name) or getInputStream(name).
 * Furthermore the MIME parameters and filename can be requested for
 * each part.
 * <pre>
 * </pre>
 *
 * @version $Id: MultiPartRequest.java,v 1.15.2.6 2003/06/04 04:47:56 starksm Exp $
 * @author  Greg Wilkins
 * @author  Jim Crossley
 */
public class MultiPartRequest
{
    /* ------------------------------------------------------------ */
    HttpServletRequest _request;
    LineInput _in;
    String _boundary;
    byte[] _byteBoundary;
    Hashtable _partMap = new Hashtable(10);
    int _char=-2;
    boolean _lastPart=false;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param request The request containing a multipart/form-data
     * request
     * @exception IOException IOException
     */
    public MultiPartRequest(HttpServletRequest request)
        throws IOException
    {
        _request=request;
        String content_type = request.getHeader(HttpFields.__ContentType);
        if (!content_type.startsWith("multipart/form-data"))
            throw new IOException("Not multipart/form-data request");

        Code.debug("Multipart content type = ",content_type);
        
        _in = new LineInput(request.getInputStream());
        
        // Extract boundary string
        _boundary="--"+
            value(content_type.substring(content_type.indexOf("boundary=")));
        
        Code.debug("Boundary=",_boundary);
        _byteBoundary= (_boundary+"--").getBytes(StringUtil.__ISO_8859_1);
        
        loadAllParts();
    }
    
    /* ------------------------------------------------------------ */
    /** Get the part names.
     * @return an array of part names
     */
    public String[] getPartNames()
    {
        return (String[]) _partMap.keySet().toArray(new String[0]);
    }
    
    /* ------------------------------------------------------------ */
    /** Check if a named part is present 
     * @param name The part
     * @return true if it was included 
     */
    public boolean contains(String name)
    {
        Part part = (Part)_partMap.get(name);
        return (part!=null);
    }
    
    /* ------------------------------------------------------------ */
    /** Get the data of a part as a string.
     * @param name The part name 
     * @return The part data
     */
    public String getString(String name)
    {
        Part part = (Part)_partMap.get(name);
        if (part==null)
            return null;
        return new String(part._data);
    }
    

    /* ------------------------------------------------------------ */
    /** Get the data of a part as a stream.
     * @param name The part name 
     * @return Stream providing the part data
     */
    public InputStream getInputStream(String name)
    {
        Part part = (Part)_partMap.get(name);
        if (part==null)
            return null;
        return new ByteArrayInputStream(part._data);
    }
    
    /* ------------------------------------------------------------ */
    /** Get the MIME parameters associated with a part.
     * @param name The part name 
     * @return Hashtable of parameters
     */
    public Hashtable getParams(String name)
    {
        Part part = (Part)_partMap.get(name);
        if (part==null)
            return null;
        return part._headers;
    }
    
    /* ------------------------------------------------------------ */
    /** Get any file name associated with a part.
     * @param name The part name 
     * @return The filename
     */
    public String getFilename(String name)
    {
        Part part = (Part)_partMap.get(name);
        if (part==null)
            return null;
        return part._filename;
    }
    
        
    
    /* ------------------------------------------------------------ */
    private void loadAllParts()
        throws IOException
    {
        // Get first boundary
        String line = _in.readLine();
        if (!line.equals(_boundary))
        {
            Code.warning(line);
            throw new IOException("Missing initial multi part boundary");
        }
        
        // Read each part
        while (!_lastPart)
        {
            // Read Part headers
            Part part = new Part();
            
            String content_disposition=null;
            while ((line=_in.readLine())!=null)
            {
                // If blank line, end of part headers
                if (line.length()==0)
                    break;

                Code.debug("LINE=",line);
                
                // place part header key and value in map
                int c = line.indexOf(':',0);
                if (c>0)
                {
                    String key = line.substring(0,c).trim().toLowerCase();
                    String value = line.substring(c+1,line.length()).trim();
                    String ev = (String) part._headers.get(key);
                    part._headers.put(key,(ev!=null)?(ev+';'+value):value);
                    Code.debug(key,": ",value);
                    if (key.equals("content-disposition"))
                        content_disposition=value;
                }
            }

            // Extract content-disposition
            boolean form_data=false;
            if (content_disposition==null)
            {
                throw new IOException("Missing content-disposition");
            }
            
            StringTokenizer tok =
                new StringTokenizer(content_disposition,";");
            while (tok.hasMoreTokens())
            {
                String t = tok.nextToken().trim();
                String tl = t.toLowerCase();
                if (t.startsWith("form-data"))
                    form_data=true;
                else if (tl.startsWith("name="))
                    part._name=value(t);
                else if (tl.startsWith("filename="))
                    part._filename=value(t);
            }

            // Check disposition
            if (!form_data)
            {
                Code.warning("Non form-data part in multipart/form-data");
                continue;
            }
            if (part._name==null || part._name.length()==0)
            {
                Code.warning("Part with no name in multipart/form-data");
                continue;
            }
            Code.debug("name=",part._name);
            Code.debug("filename=",part._filename);
            _partMap.put(part._name,part);
            part._data=readBytes();
        }       
    }

    /* ------------------------------------------------------------ */
    private byte[] readBytes()
        throws IOException
    {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();

        int c;
        boolean cr=false;
        boolean lf=false;
        
        // loop for all lines`
        while (true)
        {
            int b=0;
            while ((c=(_char!=-2)?_char:_in.read())!=-1)
            {
                _char=-2;

                // look for CR and/or LF
                if (c==13 || c==10)
                {
                    if (c==13) _char=_in.read();
                    break;
                }

                // look for boundary
                if (b>=0 && b<_byteBoundary.length && c==_byteBoundary[b])
                    b++;
                else
                {
                    // this is not a boundary
                    if (cr) baos.write(13);
                    if (lf) baos.write(10);
                    cr=lf=false;
                    
                    if (b>0)
                        baos.write(_byteBoundary,0,b);
                    b=-1;
                  
                    baos.write(c);
                }
            }

            // check partial boundary
            if ((b>0 && b<_byteBoundary.length-2) ||
                (b==_byteBoundary.length-1))
            {
                if (cr) baos.write(13);
                if (lf) baos.write(10);
                cr=lf=false;
                baos.write(_byteBoundary,0,b);
                b=-1;
            }
            
            // boundary match
            if (b>0 || c==-1)
            {
                if (b==_byteBoundary.length)
                    _lastPart=true;
                if (_char==10) _char=-2;
                break;
            }
            
            // handle CR LF
            if (cr) baos.write(13);
            if (lf) baos.write(10);
            cr=(c==13);
            lf=(c==10 || _char==10);
            if (_char==10) _char=-2;  
        }
        if (Code.verbose()) Code.debug(baos.toString());
        return baos.toByteArray();
    }
    
    
    /* ------------------------------------------------------------ */
    private String value(String nameEqualsValue)
    {   
        String value =
            nameEqualsValue.substring(nameEqualsValue.indexOf('=')+1).trim();
        
        int i=value.indexOf(';');
        if (i>0)
            value=value.substring(0,i);
        if (value.startsWith("\""))
        {
            value=value.substring(1,value.indexOf('"',1));
        }
        
        else
        {
            i=value.indexOf(' ');
            if (i>0)
                value=value.substring(0,i);
        }
        return value;
    }
    
    /* ------------------------------------------------------------ */
    private class Part
    {
        String _name=null;
        String _filename=null;
        Hashtable _headers= new Hashtable(10);
        byte[] _data=null;
    }    
};
