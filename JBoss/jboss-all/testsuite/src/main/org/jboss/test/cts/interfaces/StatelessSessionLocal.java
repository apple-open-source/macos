/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.test.cts.interfaces;

import javax.ejb.EJBLocalObject;


/** Local nterface for tests of stateless sessions
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface StatelessSessionLocal
   extends EJBLocalObject
{
   public String method1 (String msg);

   public void npeError();
}
