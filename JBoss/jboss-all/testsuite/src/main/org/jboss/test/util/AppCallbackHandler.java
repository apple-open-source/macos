package org.jboss.test.util;

import java.io.IOException;
import javax.security.auth.callback.Callback;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.callback.NameCallback;
import javax.security.auth.callback.PasswordCallback;
import javax.security.auth.callback.TextInputCallback;
import javax.security.auth.callback.UnsupportedCallbackException;
import org.jboss.security.auth.callback.ByteArrayCallback;

/** An implemeentation of the JAAS CallbackHandler interface that handles
 NameCallbacks, PasswordCallbac, TextInputCallback and the JBoss
 ByteArrayCallback

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.8.1 $
 */
public class AppCallbackHandler implements CallbackHandler
{
   private String username;
   private char[] password;
   private byte[] data;
   private String text;

   public AppCallbackHandler(String username, char[] password)
   {
      this.username = username;
      this.password = password;
   }
   public AppCallbackHandler(String username, char[] password, byte[] data)
   {
      this.username = username;
      this.password = password;
      this.data = data;
   }
   public AppCallbackHandler(String username, char[] password, byte[] data, String text)
   {
      this.username = username;
      this.password = password;
      this.data = data;
      this.text = text;
   }

   public void handle(Callback[] callbacks) throws
         IOException, UnsupportedCallbackException
   {
      for (int i = 0; i < callbacks.length; i++)
      {
         Callback c = callbacks[i];
         if( c instanceof NameCallback )
         {
            NameCallback nc = (NameCallback) c;
            nc.setName(username);
         }
         else if( c instanceof PasswordCallback )
         {
            PasswordCallback pc = (PasswordCallback) c;
            pc.setPassword(password);
         }
         else if( c instanceof TextInputCallback )
         {
            TextInputCallback tc = (TextInputCallback) c;
            tc.setText(text);
         }
         else if( c instanceof ByteArrayCallback )
         {
            ByteArrayCallback bac = (ByteArrayCallback) c;
            bac.setByteArray(data);
         }
         else
         {
            throw new UnsupportedCallbackException(c, "Unrecognized Callback");
         }
      }
   }
}

