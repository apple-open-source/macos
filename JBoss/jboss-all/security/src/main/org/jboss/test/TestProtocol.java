/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test;

import java.math.BigInteger;
import java.rmi.RemoteException;
import java.security.KeyException;
import java.security.NoSuchAlgorithmException;

import org.apache.log4j.Category;
import org.apache.log4j.ConsoleAppender;
import org.apache.log4j.NDC;
import org.apache.log4j.PatternLayout;

import org.jboss.logging.XLevel;
import org.jboss.logging.Logger;
import org.jboss.security.Util;
import org.jboss.security.srp.SRPConf;
import org.jboss.security.srp.SRPServerInterface;
import org.jboss.security.srp.SRPClientSession;
import org.jboss.security.srp.SRPParameters;
import org.jboss.security.srp.SRPServerSession;

/** Test of the SRP protocol msg exchange sequence.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.7.2.3 $
 */
public class TestProtocol extends junit.framework.TestCase
{
   static Logger log = Logger.getLogger(TestProtocol.class);
   String username = "jduke";
   char[] password = "theduke".toCharArray();
   SRPServerInterface server;
   
   /** A simple hard coded implementation of SRPServerInterface that validates
    any given username to the password and salt provided to its constructor.
    */
   static class TstImpl implements SRPServerInterface
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

      TstImpl(char[] password, String salt)
      {
         BigInteger N = SRPConf.getDefaultParams().N();
         log.trace("N: "+Util.tob64(N.toByteArray()));
         BigInteger g = SRPConf.getDefaultParams().g();
         log.trace("g: "+Util.tob64(g.toByteArray()));
         byte[] Nb = SRPConf.getDefaultParams().Nbytes();
         log.trace("N': "+Util.tob64(params.N));
         byte[] gb = SRPConf.getDefaultParams().gbytes();
         log.trace("g': "+Util.tob64(params.g));
         byte[] hn = Util.newDigest().digest(params.N);
         log.trace("H(N): "+Util.tob64(hn));
         byte[] hg = Util.newDigest().digest(params.g);
         log.trace("H(g): "+Util.tob64(hg));
         byte[] sb = Util.fromb64(salt);
         this.password = password;
         params = new SRPParameters(Nb, gb, sb);
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
   
   public TestProtocol(String name)
   {
      super(name);
   }
   
   protected void setUp() throws Exception
   {
      // Set up a simple configuration that logs on the console.
      Category root = Category.getRoot();
      root.setLevel(XLevel.TRACE);
      root.addAppender(new ConsoleAppender(new PatternLayout("%x%m%n")));
      Util.init();
      NDC.push("S,");
      server = new TstImpl(password, "123456");
      NDC.pop();
      NDC.remove();
   }
   
   public void testProtocol() throws Exception
   {
      SRPParameters params = server.getSRPParameters(username);
      NDC.push("C,");
      SRPClientSession client = new SRPClientSession(username, password, params);
      byte[] A = client.exponential();
      NDC.pop();
      NDC.push("S,");
      byte[] B = server.init(username, A);
      NDC.pop();
      NDC.push("C,");
      byte[] M1 = client.response(B);
      NDC.pop();
      NDC.push("S,");
      byte[] M2 = server.verify(username, M1);
      NDC.pop();
      NDC.push("C,");
      if( client.verify(M2) == false )
         throw new SecurityException("Failed to validate server reply");
      NDC.pop();
      NDC.remove();
   }
   
   /**
    * @param args the command line arguments
    */
   public static void main(String args[])
   {
      long start = System.currentTimeMillis();
      try
      {
         TestProtocol tst = new TestProtocol("main");
         tst.setUp();
         tst.testProtocol();
      }
      catch(Exception e)
      {
         e.printStackTrace(System.out);
      }
      finally
      {
         long end = System.currentTimeMillis();
         System.out.println("Elapsed time = "+(end - start));
      }
   }
   
}
