package org.jboss.test;

import java.math.BigInteger;
import java.security.AlgorithmParameters;
import java.security.Provider;
import java.security.SecureRandom;
import java.security.Security;
import java.util.Iterator;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SealedObject;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;

/** Tests of the Java Cryptography Extension framework
 @author Scott.Stark@jboss.org
 @version $Revision: 1.5.4.1 $
*/
public class TestJCE
{
   static void showProviders() throws Exception
   {
      Provider[] providers = Security.getProviders();
      for(int p = 0; p < providers.length; p ++)
      {
         Iterator iter = providers[p].keySet().iterator();
         System.out.println("Provider: "+providers[p].getInfo());
         while( iter.hasNext() )
         {
            String key = (String) iter.next();
            System.out.println("  key="+key+", value="+providers[p].getProperty(key));
         }
      }
   }

   static void testBlowfish() throws Exception
   {
      KeyGenerator kgen = KeyGenerator.getInstance("Blowfish");
      Cipher cipher = Cipher.getInstance("Blowfish");
      SecretKey key = null;
      int minKeyBits = -1, maxKeyBits = 0;
      int minCipherBits = -1, maxCipherBits = 0;
      for(int size = 1; size <= 448/8; size ++)
      {
         int bits = size * 8;
         try
         {
            kgen.init(bits);
            key = kgen.generateKey();
            if( minKeyBits == -1 )
               minKeyBits = bits;
            maxKeyBits = bits;
         }
         catch(Exception e)
         {
         }

         try
         {
            cipher.init(Cipher.ENCRYPT_MODE, key);
            if( minCipherBits == -1 )
               minCipherBits = bits;
            maxCipherBits = bits;
         }
         catch(Exception e)
         {
         }
      }
      System.out.println("Key range: "+minKeyBits+".."+maxKeyBits);
      System.out.println("Cipher range: "+minCipherBits+".."+maxCipherBits);
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
      
      Cipher cipher = Cipher.getInstance("Blowfish");
      AlgorithmParameters params = cipher.getParameters();
      System.out.println("Blowfish.params = "+params);
      cipher.init(Cipher.ENCRYPT_MODE, key);
      SealedObject msg = new SealedObject("This is a secret", cipher);
      
      SecretKeySpec serverKey = new SecretKeySpec(kbytes, "Blowfish");
      Cipher scipher = Cipher.getInstance("Blowfish");
      scipher.init(Cipher.DECRYPT_MODE, serverKey);
      String theMsg = (String) msg.getObject(scipher);
      System.out.println("Decrypted: "+theMsg);
      
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
         TestJCE tst = new TestJCE();
         showProviders();
         //tst.testKey();
         testBlowfish();
      }
      catch(Throwable t)
      {
         t.printStackTrace();
      }
   }
}
