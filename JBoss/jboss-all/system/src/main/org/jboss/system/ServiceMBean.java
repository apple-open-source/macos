/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

/**
 * An interface describing a JBoss service MBean.
 *
 * @see Service
 * @see ServiceMBeanSupport
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.2 $
 */
public interface ServiceMBean
   extends Service
{
   // Constants -----------------------------------------------------

   /** The JMX notification event type for a service create state */
   public static final String CREATE_EVENT = " org.jboss.system.ServiceMBean.create";
   /** The JMX notification event type for a service create state */
   public static final String DESTROY_EVENT = " org.jboss.system.ServiceMBean.destroy";

   public static final String[] states = {
      "Stopped", "Stopping", "Starting", "Started", "Failed",
      "Destroyed", "Created", "Unregistered", "Registered"
   };

   /** The Service.stop has completed */
   public static final int STOPPED  = 0;
   /** The Service.stop has been invoked */
   public static final int STOPPING = 1;
   /** The Service.start has been invoked */
   public static final int STARTING = 2;
   /** The Service.start has completed */
   public static final int STARTED  = 3;
   /** There has been an error during some operation */
   public static final int FAILED  = 4;
   /** The Service.destroy has completed */
   public static final int DESTROYED = 5;
   /** The Service.create has completed */
   public static final int CREATED = 6;
   /** The MBean has been created but has not completed MBeanRegistration.postRegister */
   public static final int UNREGISTERED = 7;
   /** The MBean has been created and has completed MBeanRegistration.postRegister */
   public static final int REGISTERED = 8;

   // Public --------------------------------------------------------
   
   String getName();
   int getState();
   String getStateString();
}
