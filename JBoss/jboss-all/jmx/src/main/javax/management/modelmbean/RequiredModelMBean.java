/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.modelmbean;

import java.lang.reflect.Constructor;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeChangeNotification;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.NotificationFilter;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.AttributeNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import javax.management.RuntimeOperationsException;
import javax.management.ListenerNotFoundException;
import javax.management.InstanceNotFoundException;

import org.jboss.mx.server.ServerConstants;

/**
 * Mandatory Model MBean implementation. The Model MBean implementation
 * can be configured by setting a <tt>jbossmx.required.modelmbean.class</tt>
 * system property.
 *
 * @see javax.management.modelmbean.ModelMBean
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2.8.2 $
 *   
 */
public class RequiredModelMBean
   implements ModelMBean, MBeanRegistration
{
   // Attributes ----------------------------------------------------
   private ModelMBean delegate = null;
   private MBeanRegistration registrationInterface = null;
   
   // Constructors --------------------------------------------------
   public RequiredModelMBean() throws MBeanException, RuntimeOperationsException
   {
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      String className = System.getProperty(
            ServerConstants.REQUIRED_MODELMBEAN_CLASS_PROPERTY,
            ServerConstants.DEFAULT_REQUIRED_MODELMBEAN_CLASS
      );

      try 
      {
         Class modelMBean = cl.loadClass(className);
         delegate = (ModelMBean)modelMBean.newInstance();  
         
         if (delegate instanceof MBeanRegistration)
            registrationInterface = (MBeanRegistration)delegate;
      }
      catch (ClassNotFoundException e)
      {
         throw new Error("Cannot instantiate model mbean class. Class " + className + " not found.");
      }
      catch (ClassCastException e) 
      {
         throw new Error("Cannot instantiate model mbean class. The target class is not an instance of ModelMBean interface.");
      }
      catch (Exception e) 
      {
         throw new Error("Cannot instantiate model mbean class " + className + " with default constructor: " + e.getMessage());      
      }
   }

   public RequiredModelMBean(ModelMBeanInfo info) throws MBeanException, RuntimeOperationsException
   {
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      String className = System.getProperty(
            ServerConstants.REQUIRED_MODELMBEAN_CLASS_PROPERTY,
            ServerConstants.DEFAULT_REQUIRED_MODELMBEAN_CLASS
      );
      
      try 
      {
         Class modelMBean = cl.loadClass(className);
         Constructor constructor = modelMBean.getConstructor(new Class[] { ModelMBeanInfo.class });
         delegate = (ModelMBean)constructor.newInstance(new Object[] { info });  
         
         if (delegate instanceof MBeanRegistration)
            registrationInterface = (MBeanRegistration)delegate;
      }
      catch (ClassNotFoundException e)
      {
         throw new Error("Cannot instantiate model mbean class. Class " + className + " not found.");
      }
      catch (ClassCastException e) 
      {
         throw new Error("Cannot instantiate model mbean class. The target class is not an instance of ModelMBean interface.");
      }
      catch (Exception e) 
      {
         throw new Error("Cannot instantiate model mbean class " + className + ": " + e.toString());      
      }
   }
   
   // ModelMBean implementation -------------------------------------
   public void setModelMBeanInfo(ModelMBeanInfo info) throws MBeanException, RuntimeOperationsException
   {
      delegate.setModelMBeanInfo(info);
   }
   
   public void setManagedResource(Object mr, String mr_type)
         throws MBeanException, RuntimeOperationsException, InstanceNotFoundException, InvalidTargetObjectTypeException
   {
      delegate.setManagedResource(mr, mr_type);   
   }

   // PersistentMBean implementation --------------------------------
   public void load() throws MBeanException, RuntimeOperationsException, InstanceNotFoundException
   {
      delegate.load();   
   }

   public void store() throws MBeanException, RuntimeOperationsException, InstanceNotFoundException
   {
      delegate.store();  
   }
   
   // DynamicMBean implementation -----------------------------------
   public MBeanInfo getMBeanInfo()
   {
      return delegate.getMBeanInfo();
   }
   
   public Object invoke(String opName, Object[] opArgs, String[] sig)
         throws MBeanException, ReflectionException
   {
      return delegate.invoke(opName, opArgs, sig);   
   }

   public Object getAttribute(String attrName)
         throws AttributeNotFoundException, MBeanException, ReflectionException
   {
      return delegate.getAttribute(attrName);
   }

   public AttributeList getAttributes(String[] attrNames) {
      return delegate.getAttributes(attrNames);
   }
   
   public void setAttribute(Attribute attribute) throws AttributeNotFoundException,
         InvalidAttributeValueException, MBeanException, ReflectionException
   {
      delegate.setAttribute(attribute);  
   }

   public AttributeList setAttributes(AttributeList attributes)
   {
      return delegate.setAttributes(attributes);
   }
   
   // ModelMBeanNotificationBroadcaster implementation --------------
   public void addNotificationListener(NotificationListener inlistener, NotificationFilter infilter, Object inhandback)
         throws IllegalArgumentException
   {
      delegate.addNotificationListener(inlistener, infilter, inhandback);  
   }

   public void removeNotificationListener(NotificationListener inlistener) throws ListenerNotFoundException
   {
      delegate.removeNotificationListener(inlistener);   
   }

   public void sendNotification(Notification ntfyObj) throws MBeanException, RuntimeOperationsException
   {
      delegate.sendNotification(ntfyObj);   
   }

   public void sendNotification(String ntfyText) throws MBeanException, RuntimeOperationsException
   {
      delegate.sendNotification(ntfyText);
   }
   
   public MBeanNotificationInfo[] getNotificationInfo() {
      return delegate.getNotificationInfo();
   }
   
   public void addAttributeChangeNotificationListener(NotificationListener inlistener,
         String inAttributeName, Object inhandback) throws MBeanException, RuntimeOperationsException, IllegalArgumentException
   {
      delegate.addAttributeChangeNotificationListener(inlistener, inAttributeName, inhandback);
   }

   public void removeAttributeChangeNotificationListener(NotificationListener inlistener, String inAttributeName)
         throws MBeanException, RuntimeOperationsException, ListenerNotFoundException
   {
      delegate.removeAttributeChangeNotificationListener(inlistener, inAttributeName);
   }

   public void sendAttributeChangeNotification(AttributeChangeNotification ntfyObj) throws MBeanException, RuntimeOperationsException
   {
      delegate.sendAttributeChangeNotification(ntfyObj);   
   }

   public void sendAttributeChangeNotification(Attribute inOldVal, Attribute inNewVal)
         throws MBeanException, RuntimeOperationsException
   {
      delegate.sendAttributeChangeNotification(inOldVal, inNewVal);   
   }

   // MBeanRegistration implementation ------------------------------
   public ObjectName preRegister(MBeanServer server, ObjectName name) throws Exception
   {
      if (registrationInterface != null)
         return registrationInterface.preRegister(server, name);
         
      return name;
   }

   public void postRegister(Boolean registrationDone)
   {
      if (registrationInterface != null)
         registrationInterface.postRegister(registrationDone);
   }
   
   public void preDeregister() throws Exception
   {
      if (registrationInterface != null)
         registrationInterface.preDeregister();
   }
   
   public void postDeregister()
   {
      if (registrationInterface != null)
         registrationInterface.postDeregister();
   }

}
