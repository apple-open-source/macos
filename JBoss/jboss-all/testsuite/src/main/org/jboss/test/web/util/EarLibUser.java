/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.web.util;

import org.jboss.test.util.Debug;

/** A class that is placed into the WEB-INF/classes directory that accesses
 * a class loaded from a jar jbosstest-web.ear/lib due to a reference from
 * the jbosstest-web-ejbs.jar manifest ClassPath.

 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
*/
public class EarLibUser
{
   public static String getClassInfo()
   {
      StringBuffer results = new StringBuffer("EarLibUser Info:\n");
      Debug.displayClassInfo(EarLibUser.class, results);
      return results.toString();
   }
}
