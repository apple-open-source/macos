// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: HttpException.java,v 1.15.2.4 2003/06/04 04:47:41 starksm Exp $
// ========================================================================

package org.mortbay.http;
import java.io.IOException;
import org.mortbay.util.TypeUtil;


/* ------------------------------------------------------------ */
/** Exception for known HTTP error status. 
 *
 * @version $Revision: 1.15.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class HttpException extends IOException
{
    private int _code;

    public int getCode()
    {
        return _code;
    }
    
    public String getReason()
    {
        return (String)HttpResponse.__statusMsg.get(TypeUtil.newInteger(_code));
    }
    
    public HttpException()
    {
        _code=HttpResponse.__400_Bad_Request ;
    }
    
    public HttpException(int code)
    {
        _code=code;
    }
    
    public HttpException(int code, String message)
    {
        super(message);
        _code=code;
    }

    public String toString()
    {
        String message=getMessage();
        String reason=getReason();
        return "HttpException("+_code+","+reason+","+message+")";
    }
}

