package org.jboss.test;

import java.security.MessageDigest;
import java.security.Security;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.crypto.JBossSXProvider;
import org.jboss.security.Util;

/** Tests of the org.jboss.crypto.*  Java Cryptography Architecture plugin
 classes
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1 $
 */
public class SecurityProviderlTestCase extends TestCase
{
   public SecurityProviderlTestCase(String name)
   {
      super(name);
   }
   
   /** Compare Util.sessionKeyHash against the SHA-SRP MessageDigest. This
    will not match the Util.sessionKeyHash as the algorithm described in
    RFC2945 does not reverse the odd and even byte arrays as is done in
    Util.sessionKeyHash.
    */
   public void testSHAInterleave() throws Exception
   {
      System.out.println("testSHAInterleave");
      MessageDigest md = MessageDigest.getInstance("SHA-SRP");
      byte[] test = "session_key".getBytes();

      byte[] hash1 = Util.sessionKeyHash(test);
      String hash1b64 = Util.encodeBase64(hash1);
      System.out.println("hash1 = "+hash1b64);
      byte[] hash2 = md.digest(test);
      String hash2b64 = Util.encodeBase64(hash2);
      System.out.println("hash2 = "+hash2b64);
      super.assertTrue(hash1b64.equals(hash2b64) == false);
   }
   /** This should match the Util.sessionKeyHash
    */
   public void testSHAReverseInterleave() throws Exception
   {
      System.out.println("testSHAReverseInterleave");
      MessageDigest md = MessageDigest.getInstance("SHA-SRP-Reverse");
      byte[] test = "session_key".getBytes();

      byte[] hash1 = Util.sessionKeyHash(test);
      String hash1b64 = Util.encodeBase64(hash1);
      System.out.println("hash1 = "+hash1b64);
      byte[] hash2 = md.digest(test);
      String hash2b64 = Util.encodeBase64(hash2);
      System.out.println("hash2 = "+hash2b64);
      super.assertEquals(hash1b64, hash2b64);
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite(SecurityProviderlTestCase.class);

      // Create an initializer for the test suite
      TestSetup wrapper = new TestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            Util.init();
            JBossSXProvider provider = new JBossSXProvider();
            Security.addProvider(provider);
         }
         protected void tearDown() throws Exception
         {
            Security.removeProvider(JBossSXProvider.PROVIDER_NAME);
         }
      };
      return wrapper;
   }

   public static void main(java.lang.String[] args)
   {
      System.setErr(System.out);
      Test suite = suite();
      junit.textui.TestRunner.run(suite);
   }
}
