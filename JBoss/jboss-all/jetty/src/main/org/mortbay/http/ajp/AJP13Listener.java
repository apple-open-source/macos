// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AJP13Listener.java,v 1.3.2.10 2003/06/04 04:47:45 starksm Exp $
// ========================================================================

package org.mortbay.http.ajp;


import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import org.mortbay.http.HttpConnection;
import org.mortbay.http.HttpListener;
import org.mortbay.http.HttpMessage;
import org.mortbay.http.HttpRequest;
import org.mortbay.http.HttpServer;
import org.mortbay.util.Code;
import org.mortbay.util.InetAddrPort;
import org.mortbay.util.Log;
import org.mortbay.util.ThreadedServer;


/* ------------------------------------------------------------ */
/** AJP 1.3 Protocol Listener.
 * This listener takes requests from the mod_jk or mod_jk2 modules
 * used by web servers such as apache and IIS to forward requests to a
 * servlet container.
 * <p>
 * This code uses the AJP13 code from tomcat3.3 as the protocol
 * specification, but is new  implementation.
 *
 * @version $Id: AJP13Listener.java,v 1.3.2.10 2003/06/04 04:47:45 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class AJP13Listener
    extends ThreadedServer
    implements HttpListener
{
    /* ------------------------------------------------------------------- */
    private HttpServer _server;
    private boolean _lastOut=false;
    private boolean _lastLow=false;
    private String _integralScheme=HttpMessage.__SSL_SCHEME;
    private String _confidentialScheme=HttpMessage.__SSL_SCHEME;
    private int _integralPort=0;
    private int _confidentialPort=0;
    private boolean _identifyListener=false;
    private int _bufferSize=8192; 
    private int _bufferReserve=512;
    private String[] _remoteServers;
    
    /* ------------------------------------------------------------------- */
    public AJP13Listener()
    {}

    /* ------------------------------------------------------------------- */
    public AJP13Listener(InetAddrPort address)
    {
        super(address);
    }
    
    /* ------------------------------------------------------------ */
    public void setHttpServer(HttpServer server)
    {
        Code.assertTrue(_server==null || _server==server,
                        "Cannot share listeners");
        _server=server;
    }

    /* ------------------------------------------------------------ */
    public HttpServer getHttpServer()
    {
        return _server;
    }

    /* ------------------------------------------------------------ */
    public int getBufferSize()
    {
        return _bufferSize;
    }
    
    /* ------------------------------------------------------------ */
    public void setBufferSize(int size)
    {
        _bufferSize=size;
        if (_bufferSize>8192)
            Code.warning("AJP Data buffer > 8192: "+size);
    }
        
    /* ------------------------------------------------------------ */
    public int getBufferReserve()
    {
        return _bufferReserve;
    }
    
    /* ------------------------------------------------------------ */
    public void setBufferReserve(int size)
    {
        _bufferReserve=size;
    }
        
    /* ------------------------------------------------------------ */
    public boolean getIdentifyListener()
    {
        return _identifyListener;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param identifyListener If true, the listener name is added to all
     * requests as the org.mortbay.http.HttListener attribute
     */
    public void setIdentifyListener(boolean identifyListener)
    {
        _identifyListener = identifyListener;
    }
    
    /* --------------------------------------------------------------- */
    public String getDefaultScheme()
    {
        return HttpMessage.__SCHEME;
    }    
    
    /* --------------------------------------------------------------- */
    public void start()
        throws Exception
    {
        super.start();
        Log.event("Started AJP13Listener on "+getInetAddrPort());
        Log.event("NOTICE: AJP13 is not a secure protocol. Please protect the port "+
                  getInetAddrPort());
    }

    /* --------------------------------------------------------------- */
    public void stop()
        throws InterruptedException
    {
        super.stop();
        Log.event("Stopped AJP13Listener on "+getInetAddrPort());
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return Array of accepted remote server hostnames or IPs.
     */
    public String[] getRemoteServers()
    {
        return _remoteServers;
    }
    
    /* ------------------------------------------------------------ */
    /** Set accepted remote servers.
     * The AJP13 protocol is not secure and contains no authentication. If
     * remote servers are set, then this listener will only accept
     * connections from hosts with matching addresses or hostnames.
     * @param servers Array of accepted remote server hostnames or IPs
     */
    public void setRemoteServers(String[] servers)
    {
        _remoteServers=servers;
    }
    
    
    /* ------------------------------------------------------------ */
    /** Handle Job.
     * Implementation of ThreadPool.handle(), calls handleConnection.
     * @param socket A Connection.
     */
    public void handleConnection(Socket socket)
        throws IOException
    {
        // Check acceptable remote servers
        if (_remoteServers!=null && _remoteServers.length>0)
        {
            boolean match=false;
            InetAddress inetAddress=socket.getInetAddress();
            String hostAddr=inetAddress.getHostAddress();
            String hostName=inetAddress.getHostName();
            for (int i=0;i<_remoteServers.length;i++)
            {
                if (hostName.equals(_remoteServers[i]) ||
                    hostAddr.equals(_remoteServers[i]))
                {
                    match=true;
                    break;
                }                    
            }
            if (!match)
            {
                Code.warning("AJP13 Connection from un-approved host: "+inetAddress);
                return;
            }
        }

        // Handle the connection
        socket.setTcpNoDelay(true);
        socket.setSoTimeout(getMaxIdleTimeMs());
        AJP13Connection connection=
            new AJP13Connection(this,
                                socket.getInputStream(),
                                socket.getOutputStream(),
                                socket,
                                getBufferSize());
        try{connection.handle();}
        finally{connection.destroy();}
    }

    /* ------------------------------------------------------------ */
    /** Customize the request from connection.
     * This method extracts the socket from the connection and calls
     * the customizeRequest(Socket,HttpRequest) method.
     * @param request
     */
    public void customizeRequest(HttpConnection connection,
                                 HttpRequest request)
    {
        if (_identifyListener)
            request.setAttribute(HttpListener.ATTRIBUTE,getName());
        
        Socket socket=(Socket)(connection.getConnection());
        customizeRequest(socket,request);
    }

    /* ------------------------------------------------------------ */
    /** Customize request from socket.
     * Derived versions of SocketListener may specialize this method
     * to customize the request with attributes of the socket used (eg
     * SSL session ids).
     * @param request
     */
    protected void customizeRequest(Socket socket,
                                    HttpRequest request)
    {
    }

    /* ------------------------------------------------------------ */
    /** Persist the connection.
     * @param connection
     */
    public void persistConnection(HttpConnection connection)
    {
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return True if low on idle threads. 
     */
    public boolean isLowOnResources()
    {
        boolean low =
            getThreads()==getMaxThreads() &&
            getIdleThreads()<getMinThreads();
        if (low && !_lastLow)
            Log.event("LOW ON THREADS: "+this);
        else if (!low && _lastLow)
        {
            Log.event("OK on threads: "+this);
            _lastOut=false;
        }
        _lastLow=low;
        return low;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return True if out of resources. 
     */
    public boolean isOutOfResources()
    {
        boolean out =
            getThreads()==getMaxThreads() &&
            getIdleThreads()==0;
        if (out && !_lastOut)
            Code.warning("OUT OF THREADS: "+this);
            
        _lastOut=out;
        return out;
    }
    
    /* ------------------------------------------------------------ */
    public boolean isIntegral(HttpConnection connection)
    {
        return ((AJP13Connection)connection).isSSL();
    }
    
    /* ------------------------------------------------------------ */
    public boolean isConfidential(HttpConnection connection)
    {
        return ((AJP13Connection)connection).isSSL();
    }

    /* ------------------------------------------------------------ */
    public String getIntegralScheme()
    {
        return _integralScheme;
    }
    
    /* ------------------------------------------------------------ */
    public void setIntegralScheme(String integralScheme)
    {
        _integralScheme = integralScheme;
    }
    
    /* ------------------------------------------------------------ */
    public int getIntegralPort()
    {
        return _integralPort;
    }

    /* ------------------------------------------------------------ */
    public void setIntegralPort(int integralPort)
    {
        _integralPort = integralPort;
    }
    
    /* ------------------------------------------------------------ */
    public String getConfidentialScheme()
    {
        return _confidentialScheme;
    }

    /* ------------------------------------------------------------ */
    public void setConfidentialScheme(String confidentialScheme)
    {
        _confidentialScheme = confidentialScheme;
    }

    /* ------------------------------------------------------------ */
    public int getConfidentialPort()
    {
        return _confidentialPort;
    }

    /* ------------------------------------------------------------ */
    public void setConfidentialPort(int confidentialPort)
    {
        _confidentialPort = confidentialPort;
    }    
}
