/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.jmx.adaptor.rmi;

import java.rmi.Remote;
import java.rmi.RemoteException;
import java.util.Set;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.ObjectName;
import javax.management.QueryExp;
import javax.management.ObjectInstance;
import javax.management.NotificationFilter;
import javax.management.MBeanInfo;

import javax.management.AttributeNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanRegistrationException;
import javax.management.NotCompliantMBeanException;
import javax.management.ReflectionException;

/**
 * RMI Interface for the server side Connector which
 * is nearly the same as the MBeanServer Interface but
 * has an additional RemoteException.
 *
 * @version <tt>$Revision: 1.2.2.1 $</tt>
 * @author  <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author  <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface RMIAdaptor 
   extends Remote
{
   Object instantiate(String className)
      throws ReflectionException, MBeanException, RemoteException;
   
   Object instantiate(String className, ObjectName loaderName) 
      throws ReflectionException, MBeanException, InstanceNotFoundException, RemoteException;
   
   Object instantiate(String className, Object[] params, String[] signature)
      throws ReflectionException, MBeanException, RemoteException;

   Object instantiate(String className,
                             ObjectName loaderName,
                             Object[] params,
                             String[] signature)
      throws ReflectionException, MBeanException, InstanceNotFoundException, RemoteException;
   
   ObjectInstance createMBean(String pClassName, ObjectName pName)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             RemoteException;
   
   ObjectInstance createMBean(String pClassName,
                              ObjectName pName,
                              ObjectName pLoaderName)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             InstanceNotFoundException,
             RemoteException;

   ObjectInstance createMBean(String pClassName,
                              ObjectName pName,
                              Object[] pParams,
                              String[] pSignature)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             RemoteException;

   ObjectInstance createMBean(String pClassName,
                              ObjectName pName,
                              ObjectName pLoaderName,
                              Object[] pParams,
                              String[] pSignature)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             InstanceNotFoundException,
             RemoteException;

   ObjectInstance registerMBean(Object object, ObjectName name) 
      throws InstanceAlreadyExistsException,
             MBeanRegistrationException,
             NotCompliantMBeanException,
             RemoteException;

   void unregisterMBean(ObjectName pName)
      throws InstanceNotFoundException,
             MBeanRegistrationException,
             RemoteException;

   ObjectInstance getObjectInstance(ObjectName pName)
      throws InstanceNotFoundException,
             RemoteException;

   Set queryMBeans(ObjectName pName, QueryExp pQuery)
      throws RemoteException;

   Set queryNames(ObjectName pName, QueryExp pQuery)
      throws RemoteException;

   boolean isRegistered(ObjectName pName)
      throws RemoteException;

   boolean isInstanceOf(ObjectName pName, String pClassName)
      throws InstanceNotFoundException,
             RemoteException;

   Integer getMBeanCount() throws RemoteException;

   Object getAttribute(ObjectName pName, String pAttribute)
      throws MBeanException,
             AttributeNotFoundException,
             InstanceNotFoundException,
             ReflectionException,
             RemoteException;

   AttributeList getAttributes(ObjectName pName,
                               String[] pAttributes)
      throws InstanceNotFoundException,
             ReflectionException,
             RemoteException;

   void setAttribute(ObjectName pName, Attribute pAttribute)
      throws InstanceNotFoundException,
             AttributeNotFoundException,
             InvalidAttributeValueException,
             MBeanException,
             ReflectionException,
             RemoteException;

   AttributeList setAttributes(ObjectName pName, AttributeList pAttributes)
      throws InstanceNotFoundException,
             ReflectionException,
             RemoteException;

   Object invoke(ObjectName pName,
                 String pActionName,
                 Object[] pParams,
                 String[] pSignature)
      throws InstanceNotFoundException,
             MBeanException,
             ReflectionException,
             RemoteException;

   String getDefaultDomain() throws RemoteException;

   MBeanInfo getMBeanInfo(ObjectName pName)
      throws InstanceNotFoundException,
             IntrospectionException,
             ReflectionException,
             RemoteException;

   void addNotificationListener(ObjectName pName,
                                ObjectName pListener,
                                NotificationFilter pFilter,
                                Object pHandback)
      throws InstanceNotFoundException,
             RemoteException;

   /**
    *
    * @param pName
    * @param listener
    * @param filter
    * @param handback
    * @throws InstanceNotFoundException
    * @throws RemoteException
    */
   void addNotificationListener(ObjectName name,
                                RMINotificationListener listener,
                                NotificationFilter filter,
                                Object handback)
      throws InstanceNotFoundException,
             RemoteException;

   void removeNotificationListener(ObjectName pName, ObjectName pListener)
      throws InstanceNotFoundException,
             ListenerNotFoundException,
             RemoteException;

   /**
    *
    * @param name
    * @param listener
    * @throws InstanceNotFoundException
    * @throws ListenerNotFoundException
    * @throws RemoteException
    */
   void removeNotificationListener(ObjectName name, RMINotificationListener listener)
      throws InstanceNotFoundException,
             ListenerNotFoundException,
             RemoteException;
}
