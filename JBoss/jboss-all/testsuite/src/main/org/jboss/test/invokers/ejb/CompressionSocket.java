package org.jboss.test.invokers.ejb;

import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.Socket;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

/** A custom socket that uses the GZIPInputStream and GZIPOutputStream streams
for compression.

@see java.net.ServerSocket
@see java.util.zip.GZIPInputStream
@see java.util.zip.GZIPOutputStream

@author  Scott_Stark@displayscape.com
@version $Revision: 1.1 $
*/
class CompressionSocket extends Socket
{

    /* InputStream used by socket */
    private InputStream in;
    /* OutputStream used by socket */
    private OutputStream out;

    /* 
     * No-arg constructor for class CompressionSocket  
     */
    public CompressionSocket()
    {
        super();
    }

    /* 
     * Constructor for class CompressionSocket 
     */
    public CompressionSocket(String host, int port) throws IOException 
    {
        super(host, port);
    }

    /* 
     * Returns a stream of type CompressionInputStream 
     */
    public InputStream getInputStream() throws IOException 
    {
        if (in == null)
        {
            in = new CompressionInputStream(super.getInputStream());
        }
        return in;
    }

    /* 
     * Returns a stream of type CompressionOutputStream 
     */
    public OutputStream getOutputStream() throws IOException 
    {
        if (out == null)
        {
            out = new CompressionOutputStream(super.getOutputStream());
        }
        return out;
    }

    /*
     * Flush the CompressionOutputStream before 
     * closing the socket.
     */
    public synchronized void close() throws IOException
    {
        OutputStream o = getOutputStream();
        o.flush();
	super.close();
    }
}
