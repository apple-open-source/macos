/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.modelmbean;

import javax.management.DynamicMBean;
import javax.management.PersistentMBean;
import javax.management.MBeanException;
import javax.management.RuntimeOperationsException;
import javax.management.InstanceNotFoundException;

/**
 * Defines Model MBean.
 *
 * @see javax.management.DynamicMBean
 * @see javax.management.PersistentMBean
 * @see javax.management.modelmbean.ModelMBeanNotificationBroadcaster
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $  
 */
public interface ModelMBean
   extends DynamicMBean, PersistentMBean, ModelMBeanNotificationBroadcaster
{

   public void setModelMBeanInfo(ModelMBeanInfo inModelMBeanInfo)
   throws MBeanException, RuntimeOperationsException;

   public void setManagedResource(Object mr, String mr_type)
   throws MBeanException, RuntimeOperationsException, InstanceNotFoundException, InvalidTargetObjectTypeException;

}

