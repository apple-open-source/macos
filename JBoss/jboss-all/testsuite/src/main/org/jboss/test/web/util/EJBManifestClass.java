/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.web.util;

/** A class that is placed into a jar in the jbossweb-test.ear/lib directory
 * and loaded by the jbosstest-web-ejbs.jar manifest ClassPath and used by
 * both the ClasspathServlet and ClasspathBean.

 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
*/
public class EJBManifestClass
{
   /** A noop method to test access to package protected methods.
    */
   void packageProtectedmethod()
   {
   }
}
