/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.test.cmp2.keygen.ejb;

import javax.ejb.EJBLocalObject;

/**
 * 
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public interface UnknownPKLocal extends EJBLocalObject
{
   public String getValue();
}
