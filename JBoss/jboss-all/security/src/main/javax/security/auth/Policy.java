/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth;

import java.security.CodeSource;
import java.security.PermissionCollection;

/** An alternate implementation of the JAAS 1.0 Configuration class that deals
 * with ClassLoader shortcomings that were fixed in the JAAS included with
 * JDK1.4 and latter. This version allows LoginModules to be loaded from the
 * Thread context ClassLoader and uses an XML based configuration by default.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 * @deprecated Use the JDK 1.4 java.security.Policy
 */
public abstract class Policy
{
   private static Policy thePolicy;

   protected Policy()
   {
   }

   public static synchronized Policy getPolicy()
   {
      return thePolicy;
   }
   public static synchronized void setPolicy(Policy policy)
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(new AuthPermission("setPolicy"));
      thePolicy = policy;
   }

   public abstract PermissionCollection getPermissions(Subject subject,
      CodeSource cs);

   public abstract void refresh();
}
