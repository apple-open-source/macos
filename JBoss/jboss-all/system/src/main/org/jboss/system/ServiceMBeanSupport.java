/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import javax.management.AttributeChangeNotification;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.NotificationBroadcasterSupport;
import javax.management.ObjectName;

import org.apache.log4j.NDC;
import org.jboss.logging.Logger;

import EDU.oswego.cs.dl.util.concurrent.SynchronizedLong;

/**
 * An abstract base class JBoss services can subclass to implement a
 * service that conforms to the ServiceMBean interface. Subclasses must
 * override {@link #getName} method and should override 
 * {@link #startService}, and {@link #stopService} as approriate.
 *
 * @see ServiceMBean
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author Scott.Stark@jboss.org
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.7.2.7 $
 */
public class ServiceMBeanSupport
   extends NotificationBroadcasterSupport
   implements ServiceMBean, MBeanRegistration
{
   /**
    * The instance logger for the service.  Not using a class logger
    * because we want to dynamically obtain the logger name from
    * concreate sub-classes.
    */
   protected Logger log;
   
   /** The MBeanServer which we have been register with. */
   protected MBeanServer server;

   /** The object name which we are registsred under. */
   protected ObjectName serviceName;

   /** The current state this service is in. */
   private int state = UNREGISTERED;

   /** Sequence number for jmx notifications we send out. */ 
   private SynchronizedLong sequenceNumber = new SynchronizedLong(0);
   
   /**
    * Construct a <t>ServiceMBeanSupport</tt>.
    *
    * <p>Sets up logging.
    */
   public ServiceMBeanSupport()
   {
      // can not call this(Class) because we need to call getClass()
      this.log = Logger.getLogger(getClass().getName());
      log.trace("Constructing");
   }

   /**
    * Construct a <t>ServiceMBeanSupport</tt>.
    *
    * <p>Sets up logging.
    *
    * @param type   The class type to determine category name from.
    */
   public ServiceMBeanSupport(final Class type)
   {
      this(type.getName());
   }
   
   /**
    * Construct a <t>ServiceMBeanSupport</tt>.
    *
    * <p>Sets up logging.
    *
    * @param category   The logger category name.
    */
   public ServiceMBeanSupport(final String category)
   {
      this(Logger.getLogger(category));
   }

   /**
    * Construct a <t>ServiceMBeanSupport</tt>.
    *
    * @param log   The logger to use.
    */
   public ServiceMBeanSupport(final Logger log)
   {
      this.log = log;
      log.trace("Constructing");
   }

   /**
    * Use the short class name as the default for the service name.
    */
   public String getName()
   {
      //
      // TODO: Check if this gets called often, if so cache this or remove if not used
      //

      // use the logger so we can better be used as a delegate instead of sub-class
      return org.jboss.util.Classes.stripPackageName(log.getName());
   }
   
   public ObjectName getServiceName()
   {
      return serviceName;
   }
   
   public MBeanServer getServer()
   {
      return server;
   }
   
   public int getState()
   {
      return state;
   }
   
   public String getStateString()
   {
      return states[state];
   }
   
   public Logger getLog()
   {
      return log;
   }


   ///////////////////////////////////////////////////////////////////////////
   //                             State Mutators                            //
   ///////////////////////////////////////////////////////////////////////////
   
   public void create() throws Exception
   {
      NDC.push(getName());
      log.debug("Creating");
      
      try
      {
         createService();
         state = CREATED;
      }
      catch (Exception e)
      {
         log.error("Initialization failed", e);
         throw e;
      }
      finally
      {
         NDC.pop();
         NDC.remove();
      }

      log.debug("Created");
   }

   public void start() throws Exception
   {
      if (state == STARTING || state == STARTED)
         return;

      state = STARTING;
      long now = System.currentTimeMillis();
      AttributeChangeNotification stateChange = new AttributeChangeNotification(this,
         getNextNotificationSequenceNumber(), now, getName()+" starting",
         "State", "java.lang.Integer",
         new Integer(STOPPED), new Integer(STARTING));
      sendNotification(stateChange);
      log.debug("Starting");
      NDC.push(getName());

      try
      {
         startService();
      }
      catch (Exception e)
      {
         state = FAILED;
         now = System.currentTimeMillis();
         stateChange = new AttributeChangeNotification(this,
            getNextNotificationSequenceNumber(), now, getName()+" failed",
            "State", "java.lang.Integer",
            new Integer(STARTING), new Integer(FAILED));
         stateChange.setUserData(e);
         sendNotification(stateChange);
         log.error("Starting failed", e);
         throw e;
      }
      finally
      {
         NDC.pop();
         NDC.remove();
      }

      state = STARTED;
      now = System.currentTimeMillis();
      stateChange = new AttributeChangeNotification(this,
         getNextNotificationSequenceNumber(), now, getName()+" started",
         "State", "java.lang.Integer",
         new Integer(STARTING), new Integer(STARTED));
      sendNotification(stateChange);
      log.info("Started " + getServiceName());
   }
   
   public void stop()
   {
      if (state != STARTED)
         return;
      
      state = STOPPING;
      long now = System.currentTimeMillis();
      AttributeChangeNotification stateChange = new AttributeChangeNotification(this,
         getNextNotificationSequenceNumber(), now, getName()+" stopping",
         "State", "java.lang.Integer",
         new Integer(STARTED), new Integer(STOPPING));
      sendNotification(stateChange);
      log.info("Stopping " + getServiceName());
      NDC.push(getName());

      try
      {
         stopService();
      }
      catch (Throwable e)
      {
         state = FAILED;
         now = System.currentTimeMillis();
         stateChange = new AttributeChangeNotification(this,
            getNextNotificationSequenceNumber(), now, getName()+" failed",
            "State", "java.lang.Integer",
            new Integer(STOPPING), new Integer(FAILED));
         stateChange.setUserData(e);
         sendNotification(stateChange);
         log.error("Stopping failed", e);
         return;
      }
      finally
      {
         NDC.pop();
         NDC.remove();
      }
      
      state = STOPPED;
      now = System.currentTimeMillis();
      stateChange = new AttributeChangeNotification(this,
         getNextNotificationSequenceNumber(), now, getName()+" stopped",
         "State", "java.lang.Integer",
         new Integer(STOPPING), new Integer(STOPPED));
      sendNotification(stateChange);
      log.debug("Stopped");
   }

   public void destroy()
   {
      if( state == DESTROYED )
         return;
      if (state != STOPPED)
         stop();
      
      log.debug("Destroying");
      
      NDC.push(getName());
      
      try
      {
         destroyService();
      }
      catch (Throwable t)
      {
         log.error("Destroying failed", t);
      }
      finally
      {
         NDC.pop();
         NDC.remove();
      }
      state = DESTROYED;
      log.debug("Destroyed");
   }


   ///////////////////////////////////////////////////////////////////////////
   //                                JMX Hooks                              //
   ///////////////////////////////////////////////////////////////////////////
   
   /**
    * Callback method of {@link MBeanRegistration}
    * before the MBean is registered at the JMX Agent.
    * 
    * <p>
    * <b>Attention</b>: Always call this method when you overwrite it in a subclass
    *                   because it saves the Object Name of the MBean.
    *
    * @param server    Reference to the JMX Agent this MBean is registered on
    * @param name      Name specified by the creator of the MBean. Note that you can
    *                  overwrite it when the given ObjectName is null otherwise the
    *                  change is discarded (maybe a bug in JMX-RI).
    */
   public ObjectName preRegister(MBeanServer server, ObjectName name)
      throws Exception
   {
      this.server = server;

      serviceName = getObjectName(server, name);
      
      return serviceName;
   }
   
   public void postRegister(Boolean registrationDone)
   {
      if (!registrationDone.booleanValue())
      {
         log.info( "Registration is not done -> stop" );
         stop();
      }
      else
      {
         state = REGISTERED;
      }
   }

   public void preDeregister() throws Exception
   {
      // nothing to do
   }
   
   public void postDeregister()
   {
      // this is presumably redundant.... 
      // ServiceController should have called stop already.
      stop();
      //clean up
      server = null;
      serviceName = null;
      state = UNREGISTERED;
   }

   /**
    * The <code>getNextNotificationSequenceNumber</code> method returns 
    * the next sequence number for use in notifications.
    *
    * @return a <code>long</code> value
    */
   protected long getNextNotificationSequenceNumber()
   {
      return sequenceNumber.increment();
   }


   ///////////////////////////////////////////////////////////////////////////
   //                       Concrete Service Overrides                      //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * Sub-classes should override this method if they only need to set their
    * object name during MBean pre-registration.
    */
   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
      throws MalformedObjectNameException
   {
      return name;
   }

   /**
    * Sub-classes should override this method to provide
    * custum 'create' logic.
    *
    * <p>This method is empty, and is provided for convenience
    *    when concrete service classes do not need to perform
    *    anything specific for this state change.
    */
   protected void createService() throws Exception {}
   
   /**
    * Sub-classes should override this method to provide
    * custum 'start' logic.
    * 
    * <p>This method is empty, and is provided for convenience
    *    when concrete service classes do not need to perform
    *    anything specific for this state change.
    */
   protected void startService() throws Exception {}
   
   /**
    * Sub-classes should override this method to provide
    * custum 'stop' logic.
    * 
    * <p>This method is empty, and is provided for convenience
    *    when concrete service classes do not need to perform
    *    anything specific for this state change.
    */
   protected void stopService() throws Exception {}
   
   /**
    * Sub-classes should override this method to provide
    * custum 'destroy' logic.
    * 
    * <p>This method is empty, and is provided for convenience
    *    when concrete service classes do not need to perform
    *    anything specific for this state change.
    */
   protected void destroyService() throws Exception {}
}
