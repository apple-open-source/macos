package org.jboss.test.invokers.ejb;

import java.io.IOException;
import java.net.Socket;
import java.net.ServerSocket;

/** A custom server socket that uses the GZIPInputStream and GZIPOutputStream
streams for compression.

@see java.net.ServerSocket
@see java.util.zip.GZIPInputStream
@see java.util.zip.GZIPOutputStream

@author  Scott_Stark@displayscape.com
@version $Revision: 1.1 $
*/
class CompressionServerSocket extends ServerSocket
{
    private boolean closed;

    public CompressionServerSocket(int port) throws IOException 
    {
        super(port);
    }

    public Socket accept() throws IOException
    {
        Socket s = new CompressionSocket();
        implAccept(s);
        return s;
    }

    public int getLocalPort()
    {
        if( closed == true )
            return -1;
        return super.getLocalPort();
    }

    public void close() throws IOException
    {
        closed = true;
        super.close();
    }
}
