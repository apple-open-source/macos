package org.jboss.test.security.interceptors;

import java.io.Serializable;
import java.security.GeneralSecurityException;
import java.util.Arrays;
import java.util.Iterator;
import java.util.Set;
import javax.crypto.Cipher;
import javax.crypto.SealedObject;
import javax.crypto.SecretKey;
import javax.crypto.spec.IvParameterSpec;
import javax.security.auth.Subject;

import org.apache.log4j.Category;

import org.jboss.invocation.Invocation;
import org.jboss.proxy.Interceptor;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.srp.SRPParameters;

/** A client side interceptor that encrypts

@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public class ClientEncryptionInterceptor
   extends Interceptor
{
   /** The is initialized the first time */
   private Cipher encryptCipher;
   private Cipher decryptCipher;
   private Category log = Category.getInstance(ClientEncryptionInterceptor.class);

   /** Creates a new instance of EncryptionInterceptor */
   public ClientEncryptionInterceptor()
   {
   }
   
   public Object invoke(Invocation mi) throws Throwable
   {
      if( encryptCipher == null )
      {
         Subject subject = SecurityAssociation.getSubject();
         initCipher(subject);
      }

      log.debug("invoke mi="+mi.getMethod());
      // Check for arguments to encrypt
      Object[] args = mi.getArguments();
      int length = args != null ? args.length : 0;
      for(int a = 0; a < length; a ++)
      {
         if( (args[a] instanceof Serializable) == false )
            continue;
         Serializable arg = (Serializable) args[a];
         SealedObject sarg = new SealedObject(arg, encryptCipher);
         args[a] = sarg;
         log.debug(" Sealed arg("+a+"): "+arg);
      }

      Interceptor next = getNext();
      Object value = next.invoke(mi);
      if( value instanceof SealedObject )
      {
         SealedObject svalue = (SealedObject) value;
         value = svalue.getObject(decryptCipher);
      }
      return value;
   }

   private void initCipher(Subject subject) throws GeneralSecurityException
   {
      Set credentials = subject.getPrivateCredentials(SecretKey.class);
      Iterator iter = credentials.iterator();
      SecretKey key = null;
      while( iter.hasNext() )
      {
         key = (SecretKey) iter.next();
      }
      if( key == null )
      {
         System.out.println("Subject: "+subject);
         throw new GeneralSecurityException("Failed to find SecretKey in Subject.PrivateCredentials");
      }

      credentials = subject.getPrivateCredentials(SRPParameters.class);
      iter = credentials.iterator();
      SRPParameters params = null;
      while( iter.hasNext() )
      {
         params = (SRPParameters) iter.next();
      }
      if( params == null )
         throw new GeneralSecurityException("Failed to find SRPParameters in Subject.PrivateCredentials");

      encryptCipher = Cipher.getInstance(key.getAlgorithm());
      IvParameterSpec iv = new IvParameterSpec(params.cipherIV);
      encryptCipher.init(Cipher.ENCRYPT_MODE, key, iv);
      decryptCipher = Cipher.getInstance(key.getAlgorithm());
      decryptCipher.init(Cipher.DECRYPT_MODE, key, iv);
   }
}
