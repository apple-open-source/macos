/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.spi;

import java.util.Map;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.spi.LoginModule;

import org.jboss.security.SecurityAssociation;
import org.jboss.security.SimplePrincipal;

/** A login module that establishes a run-as role for the duration of the login
 * phase of authentication. It can be used to allow another login module
 * interact with a secured EJB that provides authentication services.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class RunAsLoginModule implements LoginModule
{
   private String roleName;
   private boolean pushedRole;

   /** Look for the roleName option that specifies the role to use as the
    * run-as role. If not specified a default role name of nobody is used.
    */
   public void initialize(Subject subject, CallbackHandler handler,
      Map sharedState, Map options)
   {
      roleName = (String) options.get("roleName");
      if( roleName == null )
         roleName = "nobody";
   }

   /** Push the run as role using the SecurityAssociation.pushRunAsRole method
    *@see SecurityAssociation#pushRunAsRole(Principal)
    */
   public boolean login()
   {
      SimplePrincipal runAsRole = new SimplePrincipal(roleName);
      SecurityAssociation.pushRunAsRole(runAsRole);
      pushedRole = true;
      return true;
   }

   /** Calls abort to pop the run-as role
    */
   public boolean commit()
   {
      return abort();
   }

   /** Pop the run as role using the SecurityAssociation.popRunAsRole method
    *@see SecurityAssociation#popRunAsRole()
    */
   public boolean abort()
   {
      if( pushedRole == false )
         return false;

      SecurityAssociation.popRunAsRole();
      return true;
   }

   public boolean logout()
   {
      return true;
   }
}
