package org.jboss.test;

import java.math.BigInteger;
import java.security.SecureRandom;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SealedObject;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;

import org.apache.log4j.Category;
import org.apache.log4j.WriterAppender;
import org.apache.log4j.NDC;
import org.apache.log4j.PatternLayout;

import org.jboss.logging.XLevel;
import org.jboss.security.Util;
import org.jboss.security.srp.SRPParameters;
import org.jboss.security.srp.SRPClientSession;

/** Tests of using the Java Cryptography Extension framework with SRP.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.2.2.2 $
 */
public class TestJCEIntegration
{
   SimpleSRPServer server;
   SRPClientSession client;
   
   TestJCEIntegration() throws Exception
   {
      // Set up a simple configuration that logs on the console.
      Category root = Category.getRoot();
      root.setLevel(XLevel.TRACE);
      root.addAppender(new WriterAppender(new PatternLayout("%x%m%n"), System.out));
      Util.init();
      NDC.push("S,");
      server = new SimpleSRPServer("secret".toCharArray(), "123456");
      NDC.pop();
      NDC.remove();
   }
   void login(String username, char[] password) throws Exception
   {
      SRPParameters params = server.getSRPParameters(username);
      NDC.push("C,");
      client = new SRPClientSession(username, password, params);
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

   /** Performs an SRP exchange and then use the resulting session key to
    encrypt/decrypt a msg. Also test that a random key cannot be used to
    decrypt the msg.
    */
   void testSecureExchange() throws Exception
   {
      login("jduke", "secret".toCharArray());
      System.out.println("Logged into server");
      byte[] kbytes = client.getSessionKey();
      System.out.println("Session key size = "+kbytes.length);
      SecretKeySpec clientKey = new SecretKeySpec(kbytes, "Blowfish");
      System.out.println("clientKey");
      
      Cipher cipher = Cipher.getInstance("Blowfish");
      cipher.init(Cipher.ENCRYPT_MODE, clientKey);
      SealedObject msg = new SealedObject("This is a secret", cipher);
      
      // Now use the server key to decrypt the msg
      byte[] skbytes = server.session.getSessionKey();
      SecretKeySpec serverKey = new SecretKeySpec(skbytes, "Blowfish");
      Cipher scipher = Cipher.getInstance("Blowfish");
      scipher.init(Cipher.DECRYPT_MODE, serverKey);
      String theMsg = (String) msg.getObject(scipher);
      System.out.println("Decrypted: "+theMsg);

      // Try a key that should fail
      KeyGenerator kgen = KeyGenerator.getInstance("Blowfish");
      kgen.init(320);
      SecretKey key = kgen.generateKey();
      cipher.init(Cipher.DECRYPT_MODE, key);
      try
      {
         String tmp = (String) msg.getObject(cipher);
         throw new IllegalArgumentException("Should have failed to decrypt the msg");
      }
      catch(Exception e)
      {
         System.out.println("Arbitrary key failed as expected");
      }
   }

   static void testKey() throws Exception
   {
      int size = 8 * 24;
      KeyGenerator kgen = KeyGenerator.getInstance("Blowfish");
      kgen.init(size);
      SecretKey key = kgen.generateKey();
      byte[] kbytes = key.getEncoded();
      System.out.println("key.Algorithm = "+key.getAlgorithm());
      System.out.println("key.Format = "+key.getFormat());
      System.out.println("key.Encoded Size = "+kbytes.length);
      
      SecureRandom rnd = SecureRandom.getInstance("SHA1PRNG");
      BigInteger bi = new BigInteger(320, rnd);
      byte[] k2bytes = bi.toByteArray();
      SecretKeySpec keySpec = new SecretKeySpec(k2bytes, "Blowfish");
      System.out.println("key2.Algorithm = "+key.getAlgorithm());
      System.out.println("key2.Format = "+key.getFormat());
      System.out.println("key2.Encoded Size = "+kbytes.length);
   }
   
   public static void main(String[] args)
   {
      try
      {
         System.setOut(System.err);
         TestJCEIntegration tst = new TestJCEIntegration();
         tst.testSecureExchange();
         //tst.testKey();
      }
      catch(Throwable t)
      {
         t.printStackTrace();
      }
   }
}
