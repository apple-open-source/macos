/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth;

import java.security.Permission;
import java.security.PermissionCollection;

/** An alternate implementation of the JAAS 1.0 Configuration class that deals
 * with ClassLoader shortcomings that were fixed in the JAAS included with
 * JDK1.4 and latter. This version allows LoginModules to be loaded from the
 * Thread context ClassLoader and uses an XML based configuration by default.
 *
 * This permissions is used to restrict access to a Subject's private
 * credentials and the name component of the permission is of the form:
 *  "CredentialClass * *"
 * where CredentialClass is the fully qualified class name of the type of
 * credential the permission applies to. This may be "*" to indicate that all
 * credential types are included.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class PrivateCredentialPermission extends Permission
{
   /** */
   private String credentialClassName;
   /** The type of principals the permission applies. Currently always "*" */
   private String principalClass;
   /** The name of principals the permission applies. Currently always "*" */
   private String principalName;

   public PrivateCredentialPermission(String name, String actions)
   {
      super(name);
      if( actions == null || actions.equalsIgnoreCase("read") == false )
         throw new IllegalArgumentException("The only supported action is 'read'");

      // Parse CredentialClass {PrincipalClass "PrincipalName"}*
      int space = name.indexOf(' ');
      if( space > 0 )
         name = name.substring(0, space);
      credentialClassName = name;
      principalClass = "*";
      principalName = "*";
   }

   public boolean equals(Object obj)
   {
      boolean equals = false;
      if( obj instanceof PrivateCredentialPermission )
      {
         PrivateCredentialPermission pcp = (PrivateCredentialPermission) obj;
         equals = credentialClassName.equals(pcp.credentialClassName);
      }
      return equals;
   }
   public int hashCode()
   {
      return credentialClassName.hashCode();
   }
   public String getActions()
   {
      return "read";
   }
   public String getCredentialClass()
   {
      return credentialClassName;
   }
   public String[][] getPrincipals()
   {
      String[][] principals = {{"*", "*"}};
      return principals;
   }
   public boolean implies(Permission p)
   {
      if( (p instanceof PrivateCredentialPermission) == false )
         return false;
      PrivateCredentialPermission pcp = (PrivateCredentialPermission) p;
      boolean implies = credentialClassName.equals("*");
      if( implies == false )
      {
         implies = credentialClassName.equals(pcp.credentialClassName);
      }
      return implies;
   }
   public PermissionCollection newPermissionCollection()
   {
      return null;
   }
}
