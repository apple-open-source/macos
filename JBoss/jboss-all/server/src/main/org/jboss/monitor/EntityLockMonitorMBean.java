/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.monitor;

import java.util.Collection;
import javax.management.JMException;

/**
 * The JMX management interface for the {@link EntityLockMonitor} MBean.
 * 
 * @see Monitorable
 * @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.1.4.2 $
 */
public interface EntityLockMonitorMBean   
   extends org.jboss.system.ServiceMBean
{
   public String printLockMonitor();
   public void clearMonitor();
   public long getTotalContentions();
   public long getMedianWaitTime();
   public long getMaxContenders();
   public long getAverageContenders();
}
