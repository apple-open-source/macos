package org.jboss.crypto;

/** A Java2 security provider for cryptographic algorithms provided by
 the JBossSX framework.

@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public class JBossSXProvider extends java.security.Provider
{
   public static final String PROVIDER_NAME = "JBossSX";
   public static final String PROVIDER_INFO = "JBossSX Provier Version 1.0";
   public static final double PROVIDER_VERSION = 1.0;

   /** Creates a new instance of Provider */
   public JBossSXProvider()
   {
      this(PROVIDER_NAME, PROVIDER_VERSION, PROVIDER_INFO);
   }
   protected JBossSXProvider(String name, double version, String info)
   {
      super(name, version, info);
      // Setup
      super.put("MessageDigest.SHA_Interleave", "org.jboss.crypto.digest.SHAInterleave");
      super.put("Alg.Alias.MessageDigest.SHA-Interleave", "SHA_Interleave");
      super.put("Alg.Alias.MessageDigest.SHA-SRP", "SHA_Interleave");

      super.put("MessageDigest.SHA_ReverseInterleave", "org.jboss.crypto.digest.SHAReverseInterleave");
      super.put("Alg.Alias.MessageDigest.SHA-SRP-Reverse", "SHA_ReverseInterleave");
   }

}
