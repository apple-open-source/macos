/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.singleton;

import java.security.InvalidParameterException;

import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MBeanException;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.ReflectionException;

/**
 * 
 * Allows singleton MBeans to be
 * added declaratively at deployment time
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HASingletonController
  extends HASingletonSupport
  implements HASingletonControllerMBean
{

  // -------------------------------------------------------------------------
  // Constants
  // -------------------------------------------------------------------------

  // -------------------------------------------------------------------------
  // Members
  // -------------------------------------------------------------------------

  private ObjectName mSingletonMBean;
  private String mSingletonMBeanStartMethod;
  private String mSingletonMBeanStopMethod;

  /*
   * 
   * Forwards the call to the target start method
   * 
   * @see org.jboss.ha.singleton.HASingletonSupport#startSingleton()
   */
  public void startSingleton()
  {
    super.startSingleton();
    // Extending classes will implement the singleton logic here

    try
    {
      invokeMBeanMethod(
        mSingletonMBean,
        mSingletonMBeanStartMethod,
        new Object[0],
        new String[0]);
    }
    catch (JMException jme)
    {
      log.error("Controlled Singleton MBean failed to become master", jme);
    }
  }


  /*
   * 
   * Forwards the call to the target stop method
   * 
   * @see org.jboss.ha.singleton.HASingletonSupport#stopSingleton()
   */
  public void stopSingleton()
  {
    super.stopSingleton();
    // Extending classes will implement the singleton logic here

    try
    {
      invokeMBeanMethod(
        mSingletonMBean,
        mSingletonMBeanStopMethod,
        new Object[0],
        new String[0]);
    }
    catch (JMException jme)
    {
      log.error("Controlled Singleton MBean failed to become master", jme);
    }
  }

  protected Object invokeMBeanMethod (ObjectName name, 
      String operationName, Object[] params, String[] signature)
      throws InstanceNotFoundException, MBeanException, ReflectionException
  {
    return server.invoke( name, operationName, params, signature);
  }

  public String getTargetName()
  {
    return mSingletonMBean == null ? null : mSingletonMBean.toString();
  }

  public void setTargetName(String pTargetObjectName)
    throws InvalidParameterException
  {
    if (pTargetObjectName == null)
    {
      throw new InvalidParameterException("Schedulable MBean must be specified");
    }
    try
    {
      mSingletonMBean = new ObjectName(pTargetObjectName);
    }
    catch (MalformedObjectNameException mone)
    {
      log.error("Singleton MBean Object Name is malformed", mone);
      throw new InvalidParameterException("Singleton MBean is not correctly formatted");
    }
  }

  public String getTargetStartMethod()
  {
    return mSingletonMBeanStartMethod;
  }

  public void setTargetStartMethod(String pTargetStartMethod)
    throws InvalidParameterException
  {
    String lMethodName = null;
    if (pTargetStartMethod == null)
    {
      lMethodName = "";
    }
    else
    {
      lMethodName = pTargetStartMethod.trim();
    }
    if (lMethodName.equals(""))
    {
      lMethodName = "startSingleton";
    }
    mSingletonMBeanStartMethod = lMethodName;
  }


  public String getTargetStopMethod()
  {
    return mSingletonMBeanStopMethod;
  }

  public void setTargetStopMethod(String pTargetStopMethod)
    throws InvalidParameterException
  {
    String lMethodName = null;
    if (pTargetStopMethod == null)
    {
      lMethodName = "";
    }
    else
    {
      lMethodName = pTargetStopMethod.trim();
    }
    if (lMethodName.equals(""))
    {
      lMethodName = "stopSingleton";
    }
    mSingletonMBeanStopMethod = lMethodName;
  }

}
