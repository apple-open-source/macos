package org.jboss.test;

import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.security.Util;

/** Tests of the org.jboss.security.Util class
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1 $
 */
public class UtilTestCase extends TestCase
{
   public UtilTestCase(String name)
   {
      super(name);
   }
   
   /** Compare Util.encodeBase64 against the sun misc class
    */
   public void testBase64() throws Exception
   {
      System.out.println("testBase64");
      byte[] test = "echoman".getBytes();
      String b64_1 = Util.encodeBase64(test);
      System.out.println("b64_1 = "+b64_1);

      sun.misc.BASE64Encoder encoder = new sun.misc.BASE64Encoder();
      String b64_2 = encoder.encode(test);
      System.out.println("b64_2 = "+b64_2);
      super.assertEquals("encodeBase64 == BASE64Encoder", b64_1, b64_2);
   }

   /** Compare Util.encodeBase16 against the java.math.BigInteger class
    */
   public void testBase16() throws Exception
   {
      System.out.println("testBase16");
      byte[] test = "echoman".getBytes();
      String b16_1 = Util.encodeBase16(test);
      System.out.println("b16_1 = "+b16_1);

      java.math.BigInteger encoder = new java.math.BigInteger(test);
      String b16_2 = encoder.toString(16);
      System.out.println("b16_2 = "+b16_2);
      super.assertEquals("encodeBase16 == BigInteger", b16_1, b16_2);
   }

   public static void main(java.lang.String[] args)
   {
      System.setErr(System.out);
      TestSuite suite = new TestSuite(UtilTestCase.class);
      junit.textui.TestRunner.run(suite);
   }
}
