package org.jboss.test;

import org.jboss.security.Util;

/** A utility program for generating password hashes given the hashAlgorithm,
hashEncoding, and hashCharset options used by the UsernamePasswordLoginModule.
The command line usage is:
PasswordHasher [hashAlgorithm [hashEncoding [hashCharset]]] password

 @author Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public class PasswordHasher
{
   static String usage = "Usage: [hashAlgorithm [hashEncoding [hashCharset]]] password";

   /** @param args the command line arguments
    *Usage: [hashAlgorithm [hashEncoding [hashCharset]]] password
    */
   public static void main(String[] args)
   {
      String hashAlgorithm = "MD5";
      String hashEncoding = "base64";
      String hashCharset = null;
      String password = null;
      if( args.length == 0 || args[0].startsWith("-h") )
         throw new IllegalStateException(usage);
      switch( args.length )
      {
         case 4:
            hashAlgorithm = args[0];
            hashEncoding = args[1];
            hashCharset = args[2];
            password = args[3];
         break;
         case 3:
            hashAlgorithm = args[0];
            hashEncoding = args[1];
            password = args[2];
         break;
         case 2:
            hashAlgorithm = args[0];
            password = args[1];
         break;
         case 1:
            password = args[0];
         break;
      }
      String passwordHash = Util.createPasswordHash(hashAlgorithm, hashEncoding,
         hashCharset, null, password);
      System.out.println("passwordHash = "+passwordHash);
   }

}
