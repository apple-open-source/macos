// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: HttpTunnel.java,v 1.1.2.3 2003/06/04 04:47:42 starksm Exp $
// ========================================================================

package org.mortbay.http;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import org.mortbay.util.Code;
import org.mortbay.util.IO;

/* ------------------------------------------------------------ */
/** HTTP Tunnel.
 * A HTTP Tunnel can be used to take over a HTTP connection in order to
 * tunnel another protocol over it.  The prime example is the CONNECT method
 * handled by the ProxyHandler to setup a SSL tunnel between the client and
 * the real server.
 *
 * @see HttpConnection
 * @version $Revision: 1.1.2.3 $
 * @author Greg Wilkins (gregw)
 */
public class HttpTunnel
{
    private Socket _socket;
    private Thread _thread;
    private InputStream _in;
    private OutputStream _out;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    protected HttpTunnel()
    {}
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param socket The tunnel socket.
     */
    public HttpTunnel(Socket socket)
    {
        _socket=socket;
    }
    
    /* ------------------------------------------------------------ */
    /** handle method.
     * This method is called by the HttpConnection.handleNext() method if
     * this HttpTunnel has been set on that connection.
     * The default implementation of this method copies between the HTTP
     * socket and the socket passed in the constructor.
     * @param in 
     * @param out 
     */
    public void handle(InputStream in,OutputStream out)
    {
        Copy copy=new Copy();
        _in=in;
        _out=out;
        try
        {
            _thread=Thread.currentThread();
            copy.start();
            
            IO.copy(_socket.getInputStream(),_out);           
        }
        catch (Exception e)
        {
            Code.ignore(e);
        }
        finally
        {
            try
            {
                _in.close();
                _socket.shutdownOutput();
                _socket.close();
            }
            catch (Exception e){Code.ignore(e);}
            copy.interrupt();
        }
    }


    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /** Copy thread.
     * Helper thread to copy from the HTTP input to the sockets output
     */
    private class Copy extends Thread
    {
        public void run()
        {
            try
            {
                IO.copy(_in,_socket.getOutputStream());
            }
            catch (Exception e)
            {
                Code.ignore(e);
            }
            finally
            {
                try
                {
                    _out.close();
                    _socket.shutdownInput();
                    _socket.close();
                }
                catch (Exception e){Code.ignore(e);}
                _thread.interrupt();
            }
        }
    }
}
