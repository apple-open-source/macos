/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import javax.management.MBeanServer;
import javax.management.ObjectName;

/**
 * The ServiceFactory interface is used to obtain a Service
 * proxy instance for a named MBean.
 * 
 * @author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>.
 * @version $Revision: 1.1 $
 */
public interface ServiceFactory
{
   /**
    * Create a Service proxy instance for the MBean given by name.
    * 
    * @param server    The MBeanServer instance
    * @param name      The name of the MBean that wishes to be managed by
    *                  the JBoss ServiceControl service.
    */
   Service createService(MBeanServer server, ObjectName name);
}
