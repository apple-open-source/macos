package org.jboss.test;

import java.rmi.RemoteException;
import java.security.KeyException;
import java.security.NoSuchAlgorithmException;

import org.jboss.security.Util;
import org.jboss.security.srp.SRPConf;
import org.jboss.security.srp.SRPParameters;
import org.jboss.security.srp.SRPServerInterface;
import org.jboss.security.srp.SRPServerSession;

/** A simple hard coded implementation of SRPServerInterface that validates
 any given username to the password and salt provided to its constructor.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.5.4.2 $
 */
public class SimpleSRPServer implements SRPServerInterface
{
   SRPParameters params;
   SRPServerSession session;
   char[] password;

   public Object[] getSRPParameters(String username, boolean mutipleSessions)
         throws KeyException, RemoteException
   {
      return new Object[0];
   }

   public byte[] init(String username, byte[] A, int sessionID) throws SecurityException,
         NoSuchAlgorithmException, RemoteException
   {
      return new byte[0];
   }

   public byte[] verify(String username, byte[] M1, int sessionID)
         throws SecurityException, RemoteException
   {
      return new byte[0];
   }

   public byte[] verify(String username, byte[] M1, Object auxChallenge)
         throws SecurityException, RemoteException
   {
      return new byte[0];
   }

   public byte[] verify(String username, byte[] M1, Object auxChallenge, int sessionID)
         throws SecurityException, RemoteException
   {
      return new byte[0];
   }

   public void close(String username, int sessionID) throws SecurityException, RemoteException
   {
   }

   SimpleSRPServer(char[] password, String salt)
   {
      byte[] N = SRPConf.getDefaultParams().Nbytes();
      byte[] g = SRPConf.getDefaultParams().gbytes();
      byte[] s = Util.fromb64(salt);
      params = new SRPParameters(N, g, s);
      this.password = password;
   }
   
   public SRPParameters getSRPParameters(String username) throws KeyException, RemoteException
   {
      return params;
   }
   
   public byte[] init(String username,byte[] A) throws SecurityException,
      NoSuchAlgorithmException, RemoteException
   {
      // Calculate the password verfier v
      byte[] v = Util.calculateVerifier(username, password, params.s, params.N, params.g);
      // Create an SRP session
      session = new SRPServerSession(username, v, params);
      byte[] B = session.exponential();
      session.buildSessionKey(A);
      
      return B;
   }
   
   public byte[] verify(String username, byte[] M1) throws SecurityException, RemoteException
   {
      if( session.verify(M1) == false )
         throw new SecurityException("Failed to verify M1");
      return session.getServerResponse();
   }
  
   /** Close the SRP session for the given username.
    */
   public void close(String username) throws SecurityException, RemoteException
   {
   }

}
