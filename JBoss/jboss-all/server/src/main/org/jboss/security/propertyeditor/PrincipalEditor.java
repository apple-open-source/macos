/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.security.propertyeditor;

import java.beans.PropertyEditorSupport;
import java.security.Principal;

import org.jboss.security.SimplePrincipal;

/** A property editor for java.security.Principals that uses the
 * org.jboss.security.SimplePrincipal
 *
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 * @author Scott.Stark@jboss.org
 */
public class PrincipalEditor
   extends PropertyEditorSupport
{
   /** Build a SimplePrincipal
    * @param text, the name of the Principal
    */
   public void setAsText(final String text)
   {
      SimplePrincipal principal = new SimplePrincipal(text);
      setValue(principal);
   }

   /**
    * @return the name of the Principal
    */
   public String getAsText()
   {
      Principal principal = (Principal) getValue();
      return principal.getName();
   }
}
