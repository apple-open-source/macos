/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.service;

import org.jboss.system.Service;

/** A simple MBean service interface
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public interface CtsCmpServiceMBean extends Service
{
   public void setHomeJndiName(String jndiName);
}
