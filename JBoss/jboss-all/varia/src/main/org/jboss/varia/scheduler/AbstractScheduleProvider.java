/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.varia.scheduler;

import java.util.Date;

import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MBeanException;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.ha.singleton.HASingletonSupport;

/**
 * Abstract Base Class for Schedule Provider. Any Schedule
 * Provider should extend from this class or must provide
 * the MBean Interface methods.
 *
 * This class is cluster aware and allows the use of the HASingleton MBean attribute
 * to control whether or not it should run as a singleton service or not.
 * When HASingleton is set to true the MBean will usually declare dependency 
 * on a cluster partition. When not explicitly set the attribute defaults to false. 
 *
 *
 * @jmx:mbean name="jboss:service=ScheduleProvider"
 *            extends="org.jboss.ha.singleton.HASingletonMBean"
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author <a href="mailto:ivelin@apache.org">Ivelin Ivanov</a>
 * 
 * @version $Revision: 1.3.2.2 $
 */
public abstract class AbstractScheduleProvider
  extends HASingletonSupport
  implements AbstractScheduleProviderMBean
{

  // -------------------------------------------------------------------------
  // Constants
  // -------------------------------------------------------------------------

  // -------------------------------------------------------------------------
  // Members
  // -------------------------------------------------------------------------

  private ObjectName mScheduleManagerName;

  /** 
   * Determines whether or not the service will run as 
   * a singleton in a clustered environment
   **/
  private boolean mIsHASingleton = true;

  // -------------------------------------------------------------------------
  // Constructors
  // -------------------------------------------------------------------------

  /**
   * Default (no-args) Constructor
   **/
  public AbstractScheduleProvider()
  {
  }

  // -------------------------------------------------------------------------
  // SchedulerMBean Methods
  // -------------------------------------------------------------------------

  /**
   * Get the Schedule Manager Name
   *
   * @jmx:managed-operation
   */
  public String getScheduleManagerName()
  {
    return mScheduleManagerName.toString();
  }

  /**
   * Set the Schedule Manager Name
   *
   * @jmx:managed-operation
   */
  public void setScheduleManagerName(String pSchedulerManagerName)
    throws MalformedObjectNameException
  {
    mScheduleManagerName = new ObjectName(pSchedulerManagerName);
  }

  /**
   * Add the Schedules to the Schedule Manager
   *
   * @jmx:managed-operation
   */
  public abstract void startProviding() throws Exception;

  /**
   * Stops the Provider from providing and
   * causing him to remove all Schedules
   *
   * @jmx:managed-operation
   */
  public abstract void stopProviding();

  /**
   * Add a single Schedule add the Schedule Manager
   *
   * @param pTarget Object Name of the target MBean (receiver
   *                of the time notification)
   * @param pMethodName Name of the Method to be called on the
   *                    target
   * @param pMethodSignature Signature of the Method
   * @param pStart Date when the Schedule has to start
   * @param pPeriod Time between two notifications
   * @param pRepetitions Number of repetitions (-1 for unlimited)
   *
   * @return Identification of the Schedule which is used
   *         to remove it later
   **/
  protected int addSchedule(
    ObjectName pTarget,
    String pMethodName,
    String[] pMethodSignature,
    Date pStart,
    long pPeriod,
    int pRepetitions)
    throws JMException
  {
    //AS      log.info( "addScheduler(), start date: " + pStart + ", period: " + pPeriod + ", repetitions: " + pRepetitions );
    return (
      (Integer) server.invoke(
        mScheduleManagerName,
        "addSchedule",
        new Object[] {
          serviceName,
          pTarget,
          pMethodName,
          pMethodSignature,
          pStart,
          new Long(pPeriod),
          new Integer((int) pRepetitions)},
        new String[] {
          ObjectName.class.getName(),
          ObjectName.class.getName(),
          String.class.getName(),
          String[].class.getName(),
          Date.class.getName(),
          Long.TYPE.getName(),
          Integer.TYPE.getName()}))
      .intValue();
  }

  /**
   * Remove a Schedule from the Schedule Manager
   *
   * @param pID Identification of the Schedule
   **/
  protected void removeSchedule(int pID) throws JMException
  {
    server.invoke(
      mScheduleManagerName,
      "removeSchedule",
      new Object[] { new Integer(pID)},
      new String[] { Integer.TYPE.getName()});
  }

  // -------------------------------------------------------------------------
  // ServiceMBean - Methods
  // -------------------------------------------------------------------------

  /**
   * When the Service is started it will register itself at the
   * Schedule Manager which makes it necessary that the Schedule Manager
   * is already running.
   * This allows the Schedule Manager to call {@link #startProviding
   * startProviding()} which is the point to for the Provider to add
   * the Schedules on the Schedule Manager.
   * ATTENTION: If you overwrite this method in a subclass you have
   * to call this method (super.startService())
   **/
  protected void startScheduleProviderService()
    throws InstanceNotFoundException, MBeanException, ReflectionException
  {
    //  AS      log.info( "startService(), call registerProvider(), service name: " + serviceName );
    server.invoke(
      mScheduleManagerName,
      "registerProvider",
      new Object[] { serviceName.toString()},
      new String[] { String.class.getName()});
  }


  /**
   * When the Service is stopped it will unregister itself at the
   * Schedule Manager.
   * This allws the Schedule Manager to remove the Provider from its
   * list and then call {@link #stopProviding stopProviding()} which
   * is the point for the Provider to remove the Schedules from the
   * Schedule Manager.
   * ATTENTION: If you overwrite this method in a subclass you have
   * to call this method (super.stopService())
   **/
  protected void stopScheduleProviderService()
    throws InstanceNotFoundException, MBeanException, ReflectionException
  {

    try
    {
      //AS         log.info( "stopService(), call unregisterProvider()" );
      server.invoke(
        mScheduleManagerName,
        "unregisterProvider",
        new Object[] { serviceName.toString()},
        new String[] { String.class.getName()});
    }
    catch (JMException jme)
    {
      log.error(
        "Could not unregister the Provider from the Schedule Manager",
        jme);
    }
  }


  protected void startService() throws Exception
  {
    if (isHASingleton())
    {
      // if hasingleton enabled, use the HASingletonSupport logic
      super.startService();
    }
    else
    {
      startScheduleProviderService();
    }
  }

  protected void stopService() throws Exception
  {
    if (isHASingleton())
    {
      // if hasingleton enabled, use the HASingletonSupport logic
      super.stopService();
    }
    else
    {
      stopScheduleProviderService();
    }

  }



  /**
   * When HASingleton is enabled, this method will be invoked on the master node to start the singleton.
   *  
   * @see HASingletonSupport
   * 
   */
  public void startSingleton()
  {
    super.startSingleton();
    
    try
    {
      startScheduleProviderService();
    }
    catch (Exception ex)
    {
      log.error("AbstractScheduleProvider.startSingleton() failed", ex);
    }    
  }

  /**
   * When HASingleton is enabled, this method will be invoked on the master node to stop the singleton.
   *  
   * @see HASingletonSupport
   * 
   */
  public void stopSingleton()
  {
    super.stopSingleton();

    try
    {
      stopScheduleProviderService();
    }
    catch (Exception ex)
    {
      log.error("AbstractScheduleProvider.stopSingleton() failed", ex);
    }
  }


  /**
   * determines whether or not the service 
   * is clustered singleton
   * 
   * @jmx:managed-operation
   * 
   */
  public boolean isHASingleton()
  {
    return mIsHASingleton;
  }

  /**
   * determines whether or not the service 
   * is clustered singleton
   * 
   * @jmx:managed-operation
   * 
   **/
  public void setHASingleton(boolean hasingleton)
  {
    mIsHASingleton = hasingleton;
  }
  
}
