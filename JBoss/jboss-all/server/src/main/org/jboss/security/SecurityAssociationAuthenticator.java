/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.security;

import java.net.Authenticator;
import java.net.PasswordAuthentication;
import java.security.Principal;

/** An implementation of Authenticator that obtains the username and password
 * from the current SecurityAssociation state.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class SecurityAssociationAuthenticator extends Authenticator
{
   protected PasswordAuthentication getPasswordAuthentication()
   {
      Principal principal = SecurityAssociation.getPrincipal();
      Object credential = SecurityAssociation.getCredential();
      String name = principal != null ? principal.getName() : null;
      char[] password = {};
      if( credential != null )
      {
         if( password.getClass().isInstance(credential) )
            password = (char[]) credential;
         else
            password = credential.toString().toCharArray();
      }
      PasswordAuthentication auth = new PasswordAuthentication(name, password);
      return auth;
   }
}
