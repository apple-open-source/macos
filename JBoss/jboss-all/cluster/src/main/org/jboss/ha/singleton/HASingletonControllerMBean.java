/***************************************
*                                     *
*  JBoss: The OpenSource J2EE WebOS   *
*                                     *
*  Distributable under LGPL license.  *
*  See terms of license at gnu.org.   *
*                                     *
***************************************/
package org.jboss.ha.singleton;

import javax.management.ObjectName;

/** The namangement interface for the singleton controller service.
 * 
 * @see org.jboss.ha.singleton.HASingletonMBean
 * 
 * @author <a href="mailto:ivelin@apache.org">Ivelin Ivanov</a>
 * @author Scott.Stark@jboss.org
 * @author <a href="mailto:mr@gedoplan.de">Marcus Redeker</a>
 * @version $Revision: 1.1.2.4 $
 */
public interface HASingletonControllerMBean
  extends HASingletonMBean
{

  /**
   * @jmx:managed-attribute
   *
   * @return Object Name of the Target MBean for the timer notifications
   */
  public ObjectName getTargetName();

  /**
   * Sets the fully qualified JMX MBean Object Name of the Schedulable MBean to be called.
   *
   * @jmx:managed-attribute
   *
   * @param pTargetObjectName JMX MBean Object Name which should be called.
   *
   * @throws InvalidParameterException If the given value is an valid Object Name.
   */
  public void setTargetName(ObjectName targetObjectName);

  /**
   * @return start method description of the target MBean to be called
   *
   * @jmx:managed-attribute
   **/
  public String getTargetStartMethod();

  /**
   * Sets the start method name to be called on the Singleton MBean. 
   *
   * @jmx:managed-attribute
   *
   * @param pTargetStartMethod Name of the start method to be called 
   *
   * @throws InvalidParameterException If the given value is not of the right
   *                                   format
   */
  public void setTargetStartMethod(String targetStartMethod);

  /**
   * @jmx:managed-attribute
   *
   * @return start method argument of the target MBean to be called
   *
   */
  public String getTargetStartMethodArgument();

  /**
   * Sets the argument to be passed to the start method of the Singleton MBean. 
   *
   * @jmx:managed-attribute
   *
   * @param pTargetStartMethodArgument Argument value to be passed to the start method  
   *
   * @throws InvalidParameterException If the given value is not of the right
   *                                   format
   */
  public void setTargetStartMethodArgument(String targetStartMethodArgument);

  /**
   * @return stop method description of the target MBean to be called
   *
   * @jmx:managed-attribute
   **/
  public String getTargetStopMethod();

  /**
   * Sets the stop method name to be called on the Singleton MBean. 
   *
   * @jmx:managed-attribute
   *
   * @param pTargetStopMethod Name of the stop method to be called 
   *
   */
  public void setTargetStopMethod(String targetStopMethod);

  /**
   * @jmx:managed-attribute
   *
   * @return stop method argument of the target MBean to be called
   *
   */
  public String getTargetStopMethodArgument();

  /**
   * Sets the argument to be passed to the stop method of the Singleton MBean. 
   *
   * @jmx:managed-attribute
   *
   * @param pTargetStartMethodArgument Argument value to be passed to the stop method  
   *
   */
  public void setTargetStopMethodArgument(String targetStopMethodArgument);

}
