// =========================================================================== 
// $Id: UrlEncoded.java,v 1.15.2.10 2003/06/04 04:48:00 starksm Exp $
package org.mortbay.util;

import java.io.UnsupportedEncodingException;
import java.util.Iterator;
import java.util.Map;

/* ------------------------------------------------------------ */
/** Handles coding of MIME  "x-www-form-urlencoded".
 * This class handles the encoding and decoding for either
 * the query string of a URL or the content of a POST HTTP request.
 *
 * <p><h4>Notes</h4>
 * The hashtable either contains String single values, vectors
 * of String or arrays of Strings.
 *
 * This class is only partially synchronised.  In particular, simple
 * get operations are not protected from concurrent updates.
 *
 * @see java.net.URLEncoder
 * @version $Id: UrlEncoded.java,v 1.15.2.10 2003/06/04 04:48:00 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class UrlEncoded extends MultiMap
{
    /* ----------------------------------------------------------------- */
    public UrlEncoded(UrlEncoded url)
    {
        super(url);
    }
    
    /* ----------------------------------------------------------------- */
    public UrlEncoded()
    {
        super(6);
    }
    
    /* ----------------------------------------------------------------- */
    public UrlEncoded(String s)
    {
        super(6);
        decode(s,StringUtil.__ISO_8859_1);
    }
    
    /* ----------------------------------------------------------------- */
    public UrlEncoded(String s, String charset)
    {
        super(6);
        decode(s,charset);
    }
    
    /* ----------------------------------------------------------------- */
    public void decode(String query)
    {
        decodeTo(query,this,StringUtil.__ISO_8859_1);
    }
    
    /* ----------------------------------------------------------------- */
    public void decode(String query,String charset)
    {
        decodeTo(query,this,charset);
    }
    
    /* -------------------------------------------------------------- */
    /** Encode Hashtable with % encoding.
     */
    public String encode()
    {
        return encode(StringUtil.__ISO_8859_1,false);
    }
    
    /* -------------------------------------------------------------- */
    /** Encode Hashtable with % encoding.
     */
    public String encode(String charset)
    {
        return encode(charset,false);
    }
    
    /* -------------------------------------------------------------- */
    /** Encode Hashtable with % encoding.
     * @param equalsForNullValue if True, then an '=' is always used, even
     * for parameters without a value. e.g. "blah?a=&b=&c=".
     */
    public synchronized String encode(String charset, boolean equalsForNullValue)
    {
        if (charset==null)
            charset=StringUtil.__ISO_8859_1;
        
        StringBuffer result = new StringBuffer(128);
        synchronized(result)
        {
            Iterator iter = entrySet().iterator();
            while(iter.hasNext())
            {
                Map.Entry entry = (Map.Entry)iter.next();
                
                String key = entry.getKey().toString();
                Object list = entry.getValue();
                int s=LazyList.size(list);
                
                if (s==0)
                {
                    result.append(encodeString(key,charset));
                    if(equalsForNullValue)
                        result.append('=');
                }
                else
                {
                    for (int i=0;i<s;i++)
                    {
                        if (i>0)
                            result.append('&');
                        Object val=LazyList.get(list,i);
                        result.append(encodeString(key,charset));

                        if (val!=null)
                        {
                            String str=val.toString();
                            if (str.length()>0)
                            {
                                result.append('=');
                                result.append(encodeString(str,charset));
                            }
                            else if (equalsForNullValue)
                                result.append('=');
                        }
                        else if (equalsForNullValue)
                            result.append('=');
                    }
                }
                if (iter.hasNext())
                    result.append('&');
            }
            return result.toString();
        }
    }

    /* -------------------------------------------------------------- */
    /* Decoded parameters to Map.
     * @param content the string containing the encoded parameters
     * @param url The dictionary to add the parameters to
     */
    public static void decodeTo(String content,MultiMap map)
    {
        decodeTo(content,map,StringUtil.__ISO_8859_1);
    }
    


    /* -------------------------------------------------------------- */
    /** Decoded parameters to Map.
     * @param content the string containing the encoded parameters
     */
    public static void decodeTo(String content, MultiMap map, String charset)
    {
        if (charset==null)
            charset=StringUtil.__ISO_8859_1;

        synchronized(map)
        {
            String key = null;
            String value = null;
            int mark=-1;
            boolean encoded=false;
            for (int i=0;i<content.length();i++)
            {
                char c = content.charAt(i);
                switch (c)
                {
                  case '&':
                      value = encoded
                          ?decodeString(content,mark+1,i-mark-1,charset)
                          :content.substring(mark+1,i);
                      
                      mark=i;
                      encoded=false;
                      if (key != null)
                      {
                          map.add(key,value);
                          key = null;
                      }
                      break;
                  case '=':
                      if (key!=null)
                          break;
                      key = encoded
                          ?decodeString(content,mark+1,i-mark-1,charset)
                          :content.substring(mark+1,i);
                      mark=i;
                      encoded=false;
                      break;
                  case '+':
                      encoded=true;
                      break;
                  case '%':
                      encoded=true;
                      break;
                }                
            }
            
            if (key != null)
            {
                value =  encoded
                    ?decodeString(content,mark+1,content.length()-mark-1,charset)
                    :content.substring(mark+1);
                map.add(key,value);
            }
            else if (mark<content.length())
            {
                key = encoded
                    ?decodeString(content,mark+1,content.length()-mark-1,charset)
                    :content.substring(mark+1);
                map.add(key,"");
            }
        }
    }
    
    /* -------------------------------------------------------------- */
    /** Decoded parameters to Map.
     * @param data the byte[] containing the encoded parameters
     */
    public static void decodeTo(byte[] data, MultiMap map, String charset)
    {
        if (data == null || data.length == 0)
            return;

        if (charset==null)
            charset=StringUtil.__ISO_8859_1;
        
        synchronized(map)
        {
            try
            {
                int    ix = 0;
                int    ox = 0;
                String key = null;
                String value = null;
                while (ix < data.length)
                {
                    byte c = data[ix++];
                    switch ((char) c)
                    {
                      case '&':
                          value = new String(data, 0, ox, charset);
                          if (key != null)
                          {
                              map.add(key,value);
                              key = null;
                          }
                          ox = 0;
                          break;
                      case '=':
                          if (key!=null)
                              break;
                          key = new String(data, 0, ox, charset);
                          ox = 0;
                          break;
                      case '+':
                          data[ox++] = (byte)' ';
                          break;
                      case '%':
                          data[ox++] = (byte)
                              ((TypeUtil.convertHexDigit(data[ix++]) << 4)+
                               TypeUtil.convertHexDigit(data[ix++]));
                          break;
                      default:
                          data[ox++] = c;
                    }
                }
                if (key != null)
                {
                    value = new String(data, 0, ox, charset);
                    map.add(key,value);
                }
            }
            catch(UnsupportedEncodingException e)
            {
                Code.warning(e);
            }
        }
    }
    
    /* -------------------------------------------------------------- */
    /** Decode String with % encoding.
     * This method makes the assumption that the majority of calls
     * will need no decoding and uses the 8859 encoding.
     */
    public static String decodeString(String encoded)
    {
        return decodeString(encoded,0,encoded.length(),StringUtil.__ISO_8859_1);
    }
    
    /* -------------------------------------------------------------- */
    /** Decode String with % encoding.
     * This method makes the assumption that the majority of calls
     * will need no decoding.
     */
    public static String decodeString(String encoded,String charset)
    {
        return decodeString(encoded,0,encoded.length(),charset);
    }
    
            
    /* -------------------------------------------------------------- */
    /** Decode String with % encoding.
     * This method makes the assumption that the majority of calls
     * will need no decoding.
     */
    public static String decodeString(String encoded,int offset,int length,String charset)
    {
        if (charset==null)
            charset=StringUtil.__ISO_8859_1;
        byte[] bytes=null;
        int n=0;
        StringBuffer buf=null;
        
        for (int i=0;i<length;i++)
        {
            char c = encoded.charAt(offset+i);
            if (c<0||c>0xff)
                throw new IllegalArgumentException("Not decoded");
            
            if (c=='+')
            {
                if (buf==null)
                {
                    buf=new StringBuffer(length);
                    for (int j=0;j<i;j++)
                        buf.append(encoded.charAt(offset+j));
                }
                if (n>0)
                {
                    try {buf.append(new String(bytes,0,n,charset));}
                    catch(UnsupportedEncodingException e)
                    {buf.append(new String(bytes,0,n));}
                    n=0;
                }        
                buf.append(' ');
            }
            else if (c=='%' && (i+2)<length)
            {
                byte b;
                char cn = encoded.charAt(offset+i+1);
                if (cn>='a' && cn<='z')
                    b=(byte)(10+cn-'a');
                else if (cn>='A' && cn<='Z')
                    b=(byte)(10+cn-'A');
                else
                    b=(byte)(cn-'0');
                cn = encoded.charAt(offset+i+2);
                if (cn>='a' && cn<='z')
                    b=(byte)(b*16+10+cn-'a');
                else if (cn>='A' && cn<='Z')
                    b=(byte)(b*16+10+cn-'A');
                else
                    b=(byte)(b*16+cn-'0');
                
                if (buf==null)
                {
                    buf=new StringBuffer(length);
                    for (int j=0;j<i;j++)
                        buf.append(encoded.charAt(offset+j));
                }
                i+=2;
                if (bytes==null)
                    bytes=new byte[length];
                bytes[n++]=b;
            }
            else if (buf!=null)
            {
                if (n>0)
                {
                    try {buf.append(new String(bytes,0,n,charset));}
                    catch(UnsupportedEncodingException e)
                    {buf.append(new String(bytes,0,n));}
                    n=0;
                }                
                buf.append(c);
            }
        }

        if (buf==null)
        {
            if (offset==0 && encoded.length()==length)
                return encoded;
            return encoded.substring(offset,offset+length);
        }

        if (n>0)
        {
            try {buf.append(new String(bytes,0,n,charset));}
            catch(UnsupportedEncodingException e)
            {buf.append(new String(bytes,0,n));}
        }

        return buf.toString();
    }
    
    /* ------------------------------------------------------------ */
    /** Perform URL encoding.
     * Assumes 8859 charset
     * @param string 
     * @return encoded string.
     */
    public static String encodeString(String string)
    {
        return encodeString(string,StringUtil.__ISO_8859_1);
    }
    
    /* ------------------------------------------------------------ */
    /** Perform URL encoding.
     * @param string 
     * @return encoded string.
     */
    public static String encodeString(String string,String charset)
    {
        if (charset==null)
            charset=StringUtil.__ISO_8859_1;
        byte[] bytes=null;
        try
        {
            bytes=string.getBytes(charset);
        }
        catch(UnsupportedEncodingException e)
        {
            Code.warning(e);
            bytes=string.getBytes();
        }
        
        int len=bytes.length;
        byte[] encoded= new byte[bytes.length*3];
        int n=0;
        boolean noEncode=true;
        
        for (int i=0;i<len;i++)
        {
            byte b = bytes[i];
            
            if (b==' ')
            {
                noEncode=false;
                encoded[n++]=(byte)'+';
            }
            else if (b>='a' && b<='z' ||
                     b>='A' && b<='Z' ||
                     b>='0' && b<='9')
            {
                encoded[n++]=b;
            }
            else
            {
                noEncode=false;
                encoded[n++]=(byte)'%';
                byte nibble= (byte) ((b&0xf0)>>4);
                if (nibble>=10)
                    encoded[n++]=(byte)('A'+nibble-10);
                else
                    encoded[n++]=(byte)('0'+nibble);
                nibble= (byte) (b&0xf);
                if (nibble>=10)
                    encoded[n++]=(byte)('A'+nibble-10);
                else
                    encoded[n++]=(byte)('0'+nibble);
            }
        }

        if (noEncode)
            return string;
        
        try
        {    
            return new String(encoded,0,n,charset);
        }
        catch(UnsupportedEncodingException e)
        {
            Code.warning(e);
            return new String(encoded,0,n);
        }
    }


    /* ------------------------------------------------------------ */
    /** 
     */
    public Object clone()
    {
        return new UrlEncoded(this);
    }
}
