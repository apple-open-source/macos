/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.crypto;

import java.io.IOException;
import java.io.Serializable;
import java.net.Socket;
import java.rmi.server.RMIClientSocketFactory;

/** An implementation of RMIClientSocketFactory that uses the JCE Cipher
 with an SRP session key to create an encrypted stream.

@author  Scott.Stark@jboss.org
@version $Revision: 1.1.6.1 $
*/
public class CipherClientSocketFactory implements RMIClientSocketFactory, Serializable
{
   private static final long serialVersionUID = -6412485012870705607L;

   /** Creates new CipherClientSocketFactory */
   public CipherClientSocketFactory()
   {
   }

   /** Create a client socket connected to the specified host and port.
   * @param host - the host name
   * @param port - the port number
   * @return a socket connected to the specified host and port.
   * @exception IOException if an I/O error occurs during socket creation.
   */
   public Socket createSocket(String host, int port)
      throws IOException
   {
      CipherSocket socket = null;
      return socket;
   }

   public boolean equals(Object obj)
   {
      return obj instanceof CipherClientSocketFactory;
   }
   public int hashCode()
   {
      return getClass().getName().hashCode();
   }

}
