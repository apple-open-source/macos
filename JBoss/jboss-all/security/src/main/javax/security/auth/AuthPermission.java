/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth;

import java.security.BasicPermission;

/** The JAAS 1.0 classes for use of the JAAS authentication classes with
 * JDK 1.3. Use JDK 1.4+ to use the JAAS authorization classes provided by
 * the version of JAAS bundled with JDK 1.4+.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class AuthPermission extends BasicPermission
{
   public AuthPermission(String name)
   {
      super(name);
   }

   public AuthPermission(String name, String actions)
   {
      super(name, actions);
   }
}
