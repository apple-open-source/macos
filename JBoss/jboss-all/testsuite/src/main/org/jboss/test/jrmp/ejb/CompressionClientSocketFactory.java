package org.jboss.test.jrmp.ejb;

import java.io.IOException;
import java.io.Serializable;
import java.net.Socket;
import java.rmi.server.RMIClientSocketFactory;

/** The CompressionClientSocketFactory from the RMI custom socket
factory tutorial.

@author Scott_Stark@displayscape.com
@version $Revision: 1.2 $
*/
public class CompressionClientSocketFactory implements RMIClientSocketFactory, Serializable
{
    /** Create a client socket connected to the specified host and port.
     * @param host - the host name
     * @param port - the port number
     * @return a socket connected to the specified host and port.
     * @exception IOException if an I/O error occurs during socket creation.
     */
    public Socket createSocket(String host, int port) throws IOException
    {
        Socket s = new CompressionSocket(host, port);
        return s;
    }

    public boolean equals(Object obj)
    {
        return obj instanceof CompressionClientSocketFactory;
    }

    public int hashCode()
    {
        return getClass().getName().hashCode();
    }
}
