/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.ssl;

import java.io.IOException;
import java.io.Serializable;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;
import javax.net.SocketFactory;
import javax.net.ssl.HandshakeCompletedEvent;
import javax.net.ssl.HandshakeCompletedListener;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.SSLSocket;

import org.jboss.logging.Logger;

/** An implementation of SocketFactory that uses the JSSE
 default SSLSocketFactory to create a client SSLSocket.
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ClientSocketFactory extends SocketFactory
   implements HandshakeCompletedListener, Serializable
{
   //private static final long serialVersionUID = -6412485012870705607L;

   /** Creates new RMISSLClientSocketFactory */
   public ClientSocketFactory()
   {
   }

   /** Create a client socket connected to the specified host and port.
   * @param serverHost - the host name
   * @param serverPort - the port number
   * @return a socket connected to the specified host and port.
   * @exception IOException if an I/O error occurs during socket creation.
   */
   public Socket createSocket(String serverHost, int serverPort)
      throws IOException, UnknownHostException
   {
      InetAddress serverAddr = InetAddress.getByName(serverHost);
      return this.createSocket(serverAddr, serverPort);
   }

   public Socket createSocket(String serverHost, int serverPort,
      InetAddress clientAddr, int clientPort)
      throws IOException, UnknownHostException
   {
      InetAddress serverAddr = InetAddress.getByName(serverHost);
      return this.createSocket(serverAddr, serverPort, clientAddr, clientPort);
   }
   public Socket createSocket(InetAddress serverAddr, int serverPort)
      throws IOException
   {
      return this.createSocket(serverAddr, serverPort, null, 0);
   }
   public Socket createSocket(InetAddress serverAddr, int serverPort,
      InetAddress clientAddr, int clientPort)
      throws IOException
   {
      SSLSocketFactory factory = (SSLSocketFactory) SSLSocketFactory.getDefault();
      SSLSocket socket = (SSLSocket) factory.createSocket(serverAddr, serverPort, clientAddr, clientPort);
      socket.addHandshakeCompletedListener(this);
      return socket;
   }

   public boolean equals(Object obj)
   {
      return obj instanceof ClientSocketFactory;
   }
   public int hashCode()
   {
      return getClass().getName().hashCode();
   }

   public void handshakeCompleted(HandshakeCompletedEvent handshakeCompletedEvent)
   {
      Logger log = Logger.getLogger(ClientSocketFactory.class);
      if( log.isTraceEnabled() )
      {
         String cipher = handshakeCompletedEvent.getCipherSuite();
         SSLSession session = handshakeCompletedEvent.getSession();
         String peerHost = session.getPeerHost();
         log.debug("SSL handshakeCompleted, cipher="+cipher
            +", peerHost="+peerHost);
      }
   }

}
