/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.test.cmp2.keygen.ejb;

import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

/**
 * 
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public interface UnknownPKLocalHome extends EJBLocalHome
{
   public UnknownPKLocal create(String value) throws CreateException;
   public UnknownPKLocal findByPrimaryKey(Object pk) throws FinderException;
}
