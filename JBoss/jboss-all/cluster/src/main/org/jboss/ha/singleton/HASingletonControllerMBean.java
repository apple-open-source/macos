/***************************************
*                                     *
*  JBoss: The OpenSource J2EE WebOS   *
*                                     *
*  Distributable under LGPL license.  *
*  See terms of license at gnu.org.   *
*                                     *
***************************************/
package org.jboss.ha.singleton;

/**
 * 
 * @see org.jboss.ha.singleton.HASingletonMBean
 * 
 * @author <a href="mailto:ivelin@apache.org">Ivelin Ivanov</a>
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b></p>
 * 
 */

public interface HASingletonControllerMBean
  extends HASingletonMBean
{

  /**
   * @jmx:managed-attribute
   *
   * @return Object Name of the Target MBean for the timer notifications
   */
  public String getTargetName();

  /**
   * Sets the fully qualified JMX MBean Object Name of the Schedulable MBean to be called.
   *
   * @jmx:managed-attribute
   *
   * @param pTargetObjectName JMX MBean Object Name which should be called.
   *
   * @throws InvalidParameterException If the given value is an valid Object Name.
   */
  public void setTargetName(String pTargetObjectName);

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
  public void setTargetStartMethod(String pTargetStartMethod);

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
   * @throws InvalidParameterException If the given value is not of the right
   *                                   format
   */
  public void setTargetStopMethod(String pTargetStopMethod);

}
