/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.net.jmx.adaptor.server;

import org.jboss.system.ServiceMBeanSupport;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.loading.ClassLoaderRepository;

import java.net.URL;
import java.net.URLClassLoader;
import java.net.MalformedURLException;

import org.jboss.mx.loading.UnifiedClassLoader;

/**
 * wrapper around the mbean server that may be exposed as a
 * soap-enabled mbean to the outside world.
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created October 1, 2001
 * @version $Revision: 1.8.2.2 $
 */

public class Adaptor extends ServiceMBeanSupport implements AdaptorMBean, Constants {

    public void startService() throws Exception {
    }

    public void stopService() {
    }

    public java.lang.Object instantiate(java.lang.String str, javax.management.ObjectName objectName) throws javax.management.ReflectionException, javax.management.MBeanException, javax.management.InstanceNotFoundException {
        return getServer().instantiate(str,objectName);
    }

    public boolean isInstanceOf(javax.management.ObjectName objectName, java.lang.String str) throws javax.management.InstanceNotFoundException {
        return getServer().isInstanceOf(objectName,str);
    }

    public javax.management.ObjectInstance registerMBean(java.lang.Object obj, javax.management.ObjectName objectName) throws javax.management.InstanceAlreadyExistsException, javax.management.MBeanRegistrationException, javax.management.NotCompliantMBeanException {
        return getServer().registerMBean(obj,objectName);
    }

    public java.lang.String getDefaultDomain() {
        return getServer().getDefaultDomain();
    }

    public javax.management.MBeanInfo getMBeanInfo(javax.management.ObjectName objectName) throws javax.management.InstanceNotFoundException, javax.management.IntrospectionException, javax.management.ReflectionException {
        return getServer().getMBeanInfo(objectName);
    }

    public javax.management.ObjectInstance getObjectInstance(javax.management.ObjectName objectName) throws javax.management.InstanceNotFoundException {
        return getServer().getObjectInstance(objectName);
    }

    public java.lang.Object instantiate(java.lang.String str) throws javax.management.ReflectionException, javax.management.MBeanException {
        return getServer().instantiate(str);
    }

    public boolean isRegistered(javax.management.ObjectName objectName) {
        return getServer().isRegistered(objectName);
    }

    public void addNotificationListener(javax.management.ObjectName objectName, javax.management.NotificationListener notificationListener, javax.management.NotificationFilter notificationFilter, java.lang.Object obj) throws javax.management.InstanceNotFoundException {
        getServer().addNotificationListener(objectName,notificationListener,notificationFilter,obj);
    }

    public void addNotificationListener(javax.management.ObjectName objectName, javax.management.ObjectName objectName1, javax.management.NotificationFilter notificationFilter, java.lang.Object obj) throws javax.management.InstanceNotFoundException {
        getServer().addNotificationListener(objectName,objectName1,notificationFilter,obj);
    }

    public javax.management.ObjectInstance createMBean(java.lang.String str, javax.management.ObjectName objectName) throws javax.management.ReflectionException, javax.management.InstanceAlreadyExistsException, javax.management.MBeanRegistrationException, javax.management.MBeanException, javax.management.NotCompliantMBeanException {
        return getServer().createMBean(str,objectName);
    }

    public javax.management.ObjectInstance createMBean(java.lang.String str, javax.management.ObjectName objectName, javax.management.ObjectName objectName2) throws javax.management.ReflectionException, javax.management.InstanceAlreadyExistsException, javax.management.MBeanRegistrationException, javax.management.MBeanException, javax.management.NotCompliantMBeanException, javax.management.InstanceNotFoundException {
        return getServer().createMBean(str,objectName,objectName2);
    }

    public java.lang.Object getAttribute(javax.management.ObjectName objectName, java.lang.String str) throws javax.management.MBeanException, javax.management.AttributeNotFoundException, javax.management.InstanceNotFoundException, javax.management.ReflectionException {
        return getServer().getAttribute(objectName,str);
    }

    public javax.management.ObjectInstance createMBean(java.lang.String str, javax.management.ObjectName objectName, javax.management.ObjectName objectName2, java.lang.Object[] obj, java.lang.String[] str4) throws javax.management.ReflectionException, javax.management.InstanceAlreadyExistsException, javax.management.MBeanRegistrationException, javax.management.MBeanException, javax.management.NotCompliantMBeanException, javax.management.InstanceNotFoundException {
        return getServer().createMBean(str,objectName,objectName2, obj, str4);
    }

    public javax.management.ObjectInstance createMBean(java.lang.String str, javax.management.ObjectName objectName, java.lang.Object[] obj, java.lang.String[] str3) throws javax.management.ReflectionException, javax.management.InstanceAlreadyExistsException, javax.management.MBeanRegistrationException, javax.management.MBeanException, javax.management.NotCompliantMBeanException {
        return getServer().createMBean(str,objectName,obj,str3);
    }

    public void setAttribute(javax.management.ObjectName objectName, javax.management.Attribute attribute) throws javax.management.InstanceNotFoundException, javax.management.AttributeNotFoundException, javax.management.InvalidAttributeValueException, javax.management.MBeanException, javax.management.ReflectionException {
        getServer().setAttribute(objectName,attribute);
    }

    public java.lang.Object instantiate(java.lang.String str, java.lang.Object[] obj, java.lang.String[] str2) throws javax.management.ReflectionException, javax.management.MBeanException {
        return getServer().instantiate(str,obj,str2);
    }

    public java.lang.Object instantiate(java.lang.String str, javax.management.ObjectName objectName, java.lang.Object[] obj, java.lang.String[] str3) throws javax.management.ReflectionException, javax.management.MBeanException, javax.management.InstanceNotFoundException {
        return getServer().instantiate(str,objectName,obj,str3);
    }

    public java.io.ObjectInputStream deserialize(java.lang.String str, javax.management.ObjectName objectName, byte[] values) throws javax.management.InstanceNotFoundException, javax.management.OperationsException, javax.management.ReflectionException {
        return getServer().deserialize(str,objectName,values);
    }

    public java.io.ObjectInputStream deserialize(java.lang.String str, byte[] values) throws javax.management.OperationsException, javax.management.ReflectionException {
        return getServer().deserialize(str,values);
    }

    public java.util.Set queryMBeans(javax.management.ObjectName objectName, javax.management.QueryExp queryExp) {
        return getServer().queryMBeans(objectName,queryExp);
    }

    public javax.management.AttributeList setAttributes(javax.management.ObjectName objectName, javax.management.AttributeList attributeList) throws javax.management.InstanceNotFoundException, javax.management.ReflectionException {
        return getServer().setAttributes(objectName,attributeList);
    }

    public java.lang.Integer getMBeanCount() {
        return getServer().getMBeanCount();
    }

    public java.lang.Object invoke(javax.management.ObjectName objectName, java.lang.String str, java.lang.Object[] obj, java.lang.String[] str3) throws javax.management.InstanceNotFoundException, javax.management.MBeanException, javax.management.ReflectionException {
        return getServer().invoke(objectName,str,obj,str3);
    }

    public java.io.ObjectInputStream deserialize(javax.management.ObjectName objectName, byte[] values) throws javax.management.InstanceNotFoundException, javax.management.OperationsException {
        return getServer().deserialize(objectName,values);
    }

    public javax.management.AttributeList getAttributes(javax.management.ObjectName objectName, java.lang.String[] str) throws javax.management.InstanceNotFoundException, javax.management.ReflectionException {
        return getServer().getAttributes(objectName,str);
    }

    public java.util.Set queryNames(javax.management.ObjectName objectName, javax.management.QueryExp queryExp) {
        return getServer().queryNames(objectName,queryExp);
    }

    public void unregisterMBean(javax.management.ObjectName objectName) throws javax.management.InstanceNotFoundException, javax.management.MBeanRegistrationException {
        getServer().unregisterMBean(objectName);
    }

    public void removeNotificationListener(javax.management.ObjectName objectName, javax.management.ObjectName objectName1) throws javax.management.InstanceNotFoundException, javax.management.ListenerNotFoundException {
        getServer().removeNotificationListener(objectName,objectName1);
    }

    public void removeNotificationListener(javax.management.ObjectName objectName, javax.management.NotificationListener notificationListener) throws javax.management.InstanceNotFoundException, javax.management.ListenerNotFoundException {
        getServer().removeNotificationListener(objectName, notificationListener);
    }

   // MBeanServer JMX 1.2 implementation ------------------------------

   public String[] getDomains()
   {
      throw new UnsupportedOperationException();
   }

   public void removeNotificationListener(ObjectName target, ObjectName listener, NotificationFilter filter, Object handback)
   {
      throw new UnsupportedOperationException();
   }

   public void removeNotificationListener(ObjectName target, NotificationListener listener, NotificationFilter filter, Object handback)
   {
      throw new UnsupportedOperationException();
   }

   public ClassLoaderRepository getClassLoaderRepository()
   {
      throw new UnsupportedOperationException();
   }

   public ClassLoader getClassLoader(ObjectName name)
   {
      throw new UnsupportedOperationException();
   }

   public ClassLoader getClassLoaderFor(ObjectName name)
   {
      throw new UnsupportedOperationException();
   }
}
