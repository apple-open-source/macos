/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import java.util.Set;
import java.util.Iterator;

import javax.security.auth.Subject;

import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.SecurityException;
import javax.resource.spi.ConnectionRequestInfo;

import javax.resource.spi.security.PasswordCredential;

/**
 * ???
 *
 * Created: Sat Mar 31 03:23:30 2001
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version $Revision: 1.2 $
 */
public class JmsCred
{
   public String name;
   public String pwd;
    
   public JmsCred() {
      // empty
   }

   /**
    * Get our own simple cred
    */
   public static JmsCred getJmsCred(ManagedConnectionFactory mcf,
                                    Subject subject, 
                                    ConnectionRequestInfo info) 
      throws SecurityException
   {
      JmsCred jc = new JmsCred();
      if (subject == null && info !=null )
      {
         // Credentials specifyed on connection request
         jc.name = ((JmsConnectionRequestInfo)info).getUserName();
         jc.pwd = ((JmsConnectionRequestInfo)info).getPassword();
      }
      else if (subject != null)
      {
         // Credentials from appserver
         Set creds = 
            subject.getPrivateCredentials(PasswordCredential.class);
         PasswordCredential pwdc = null;
         Iterator credentials = creds.iterator();
         while (credentials.hasNext())
         {
            PasswordCredential curCred = 
               (PasswordCredential) credentials.next();
            if (curCred.getManagedConnectionFactory().equals(mcf)) {
               pwdc = curCred;
               break;
            }
         }
         
         if (pwdc == null) {
            // No hit - we do need creds
            throw new SecurityException("No Passwdord credentials found");
         }
         jc.name = pwdc.getUserName();
         jc.pwd = new String(pwdc.getPassword());
      }
      else {
         throw new SecurityException("No Subject or ConnectionRequestInfo set, could not get credentials");
      }
      return jc;
   }

   public String toString()
   {
      return super.toString() + "{ username=" + name + ", password=" + pwd + " }";
   }
}
