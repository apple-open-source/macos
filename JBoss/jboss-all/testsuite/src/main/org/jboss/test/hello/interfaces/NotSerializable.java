/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.test.hello.interfaces;

import java.io.Serializable;

/**
 *  @author adrian@jboss.org
 *  @version $Revision: 1.1.4.1 $
 */
public class NotSerializable implements Serializable
{
   // This means the class fails to implement serializable correctly
   public Object bad = new Object();
}
