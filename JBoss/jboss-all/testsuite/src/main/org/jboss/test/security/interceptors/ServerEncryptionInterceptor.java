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

import org.jboss.ejb.Container;
import org.jboss.ejb.Interceptor;
import org.jboss.ejb.plugins.AbstractInterceptor;
import org.jboss.invocation.Invocation;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.srp.SRPParameters;

/** A server side interceptor that encrypts

@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public class ServerEncryptionInterceptor extends AbstractInterceptor
{
   /** The is initialized the first time */
   private Cipher decryptCipher;
   private Cipher encryptCipher;
   private Container container;

   /** Creates a new instance of EncryptionInterceptor */
   public ServerEncryptionInterceptor()
   {
   }

   public void setContainer(Container container)
   {
      this.container = container;
   }

   public Container getContainer()
   {
      return container;
   }

   public Object invoke(Invocation mi) throws Exception
   {
      if( decryptCipher == null )
      {
         Subject subject = SecurityAssociation.getSubject();
         initCipher(subject);
      }

      log.debug("invoke mi="+mi.getMethod());
      // Check for arguments to decrypt
      Object[] args = mi.getArguments();
      int length = args != null ? args.length : 0;
      for(int a = 0; a < length; a ++)
      {
         if( (args[a] instanceof SealedObject) == false )
            continue;
         SealedObject sarg = (SealedObject) args[a];
         Object arg = sarg.getObject(decryptCipher);
         args[a] = arg;
         log.debug(" Unsealed arg("+a+"): "+arg);
      }
      // We must set the arguments because args[] may be a copy
      mi.setArguments(args);

      Interceptor next = getNext();
      Object value = next.invoke(mi);
      if( value instanceof Serializable )
      {
         Serializable svalue = (Serializable) value;
         value = new SealedObject(svalue, encryptCipher);
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
         throw new GeneralSecurityException("Failed to find SecretKey in Subject.PrivateCredentials");

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
