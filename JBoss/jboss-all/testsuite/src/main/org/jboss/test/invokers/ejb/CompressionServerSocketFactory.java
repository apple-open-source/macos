package org.jboss.test.invokers.ejb;

import java.io.IOException;
import java.io.Serializable;
import java.net.ServerSocket;
import java.rmi.server.RMIServerSocketFactory;

/** The CompressionServerSocketFactory from the RMI custom socket
factory tutorial.

@author Scott_Stark@displayscape.com
@version $Revision: 1.1 $
*/
public class CompressionServerSocketFactory implements RMIServerSocketFactory, Serializable
{
    /**
     * Create a server socket on the specified port (port 0 indicates
     * an anonymous port).
     * @param  port the port number
     * @return the server socket on the specified port
     * @exception IOException if an I/O error occurs during server socket
     * creation
     * @since 1.2
     */
    public ServerSocket createServerSocket(int port) throws IOException
    {
        ServerSocket activeSocket = new CompressionServerSocket(port);
        return activeSocket;
    }

    public boolean equals(Object obj)
    {
        return obj instanceof CompressionServerSocketFactory;
    }

    public int hashCode()
    {
        return getClass().getName().hashCode();
    }
}
