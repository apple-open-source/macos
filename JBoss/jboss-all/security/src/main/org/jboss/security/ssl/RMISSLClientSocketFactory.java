/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.ssl;

import java.io.IOException;
import java.io.Serializable;
import java.rmi.server.RMIClientSocketFactory;
import javax.net.ssl.HandshakeCompletedEvent;
import javax.net.ssl.HandshakeCompletedListener;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.SSLSocket;

import org.jboss.logging.Logger;

/** An implementation of RMIClientSocketFactory that uses the JSSE
 default SSLSocketFactory to create a client SSLSocket.
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.3.4.1 $
 */
public class RMISSLClientSocketFactory implements HandshakeCompletedListener,
   RMIClientSocketFactory, Serializable
{
   private static final long serialVersionUID = -6412485012870705607L;

   /** Creates new RMISSLClientSocketFactory */
   public RMISSLClientSocketFactory()
   {
   }

   /** Create a client socket connected to the specified host and port.
   * @param host - the host name
   * @param port - the port number
   * @return a socket connected to the specified host and port.
   * @exception IOException if an I/O error occurs during socket creation.
   */
   public java.net.Socket createSocket(String host, int port)
      throws IOException
   {
      SSLSocketFactory factory = (SSLSocketFactory) SSLSocketFactory.getDefault();
      SSLSocket socket = (SSLSocket) factory.createSocket(host, port);
      socket.addHandshakeCompletedListener(this);
      return socket;
   }

   public boolean equals(Object obj)
   {
      return obj instanceof RMISSLClientSocketFactory;
   }
   public int hashCode()
   {
      return getClass().getName().hashCode();
   }

   public void handshakeCompleted(HandshakeCompletedEvent handshakeCompletedEvent)
   {
      Logger log = Logger.getLogger(RMISSLClientSocketFactory.class);
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
