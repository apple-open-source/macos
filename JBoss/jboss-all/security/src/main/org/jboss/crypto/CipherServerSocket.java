package org.jboss.crypto;

import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import javax.crypto.SecretKey;

/**
 *
 * @author  Scott.Stark@jboss.org
 */
public class CipherServerSocket extends ServerSocket
{
   String algorithm;
   SecretKey key;

   /** Creates a new instance of CipherServerSocket */
   public CipherServerSocket(int port, int backlog,
      InetAddress bindAddr, String algorithm, SecretKey key) throws IOException
   {
      super(port, backlog, bindAddr);
      this.algorithm = algorithm;
      this.key = key;
   }

   public Socket accept() throws IOException
   {
      Socket s = super.accept();
      return new CipherSocket(s, algorithm, key);
   }
}
