/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth.callback;

import java.io.Serializable;
import java.util.Arrays;

/** The JAAS 1.0 classes for use of the JAAS authentication classes with
 * JDK 1.3. Use JDK 1.4+ to use the JAAS authorization classes provided by
 * the version of JAAS bundled with JDK 1.4+.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class PasswordCallback implements Callback, Serializable
{
   private String prompt;
   private boolean echoOn;
   private char[] password;

   public PasswordCallback(String prompt, boolean echoOn)
   {
      this.prompt = prompt;
      this.echoOn = echoOn;
   }

   public String getPrompt()
   {
      return prompt;
   }
   public boolean isEchoOn()
   {
      return echoOn;
   }

   public void clearPassword()
   {
      Arrays.fill(password, ' ');
   }
   public char[] getPassword()
   {
      char[] tmpPassword = null;
      if( password != null )
         tmpPassword = (char[]) password.clone();
      return tmpPassword;
   }
   public void setPassword(char[] password)
   {
      if( password != null )
         password = (char[]) password.clone();
      this.password = password;
   }
}
