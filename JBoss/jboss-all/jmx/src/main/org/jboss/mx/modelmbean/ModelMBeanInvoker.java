/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.modelmbean;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.beans.BeanInfo;
import java.beans.Introspector;
import java.beans.PropertyDescriptor;
import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;
import java.util.HashMap;
import javax.management.AttributeChangeNotification;
import javax.management.AttributeChangeNotificationFilter;
import javax.management.Attribute;
import javax.management.Descriptor;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.NotificationBroadcasterSupport;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MBeanException;
import javax.management.ListenerNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.RuntimeOperationsException;
import javax.management.IntrospectionException;
import javax.management.MBeanAttributeInfo;
import javax.management.JMException;
import javax.management.NotificationBroadcaster;

import javax.management.modelmbean.ModelMBean;
import javax.management.modelmbean.ModelMBeanInfo;
import javax.management.modelmbean.ModelMBeanInfoSupport;
import javax.management.modelmbean.InvalidTargetObjectTypeException;
import javax.management.modelmbean.ModelMBeanAttributeInfo;

import org.jboss.logging.Logger;
import org.jboss.mx.server.MBeanInvoker;
import org.jboss.mx.interceptor.Interceptor;
import org.jboss.mx.interceptor.MBeanAttributeInterceptor;
import org.jboss.mx.persistence.NullPersistence;
import org.jboss.mx.interceptor.ObjectReferenceInterceptor;
import org.jboss.mx.interceptor.PersistenceInterceptor2;
import org.jboss.mx.persistence.PersistenceManager;

/**
 * An abstract base class that can be used to implement different
 * Model MBean implementations.
 *
 * @see javax.management.modelmbean.ModelMBean
 * @see org.jboss.mx.modelmbean.ModelMBeanConstants
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author Matt Munz
 * @author Scott.Stark@jboss.org
 * @author <a href="mailto:julien@jboss.org">Julien Viet</a>
 * @version $Revision: 1.1.4.10 $
 */
public abstract class ModelMBeanInvoker
      extends MBeanInvoker
      implements ModelMBean, ModelMBeanConstants, MBeanRegistration
{
   Logger log = Logger.getLogger(ModelMBeanInvoker.class.getName());

   // Attributes ----------------------------------------------------
   protected String resourceType = null;
   protected PersistenceManager persistence = new NullPersistence();
   protected ReflectedNotificationBroadcaster notifier = ReflectedNotificationBroadcaster.with(new NotificationBroadcasterSupport());

   protected long notifierSequence = 1;
   protected long attrNotifierSequence = 1;


   // Constructors --------------------------------------------------
   public ModelMBeanInvoker()
   {
   }

   public ModelMBeanInvoker(ModelMBeanInfo info) throws MBeanException
   {
      setModelMBeanInfo(info);

      try
      {
         load();
      }
      catch (InstanceNotFoundException e)
      {
         throw new MBeanException(e);
      }
   }

   // Public --------------------------------------------------------

   // verify the resource types supported by the concrete MMBean implementation
   public abstract boolean isSupportedResourceType(Object resource, String resourceType);


   // ModelMBean implementation -------------------------------------
   public void setModelMBeanInfo(ModelMBeanInfo info) throws MBeanException, RuntimeOperationsException
   {
      if (info == null)
         throw new IllegalArgumentException("MBeanInfo cannot be null.");

      this.info = new ModelMBeanInfoSupport(info);
      setDescriptors(info.getDescriptors(ALL_DESCRIPTORS));
   }

   public void setManagedResource(Object ref, String resourceType)
         throws MBeanException, InstanceNotFoundException, InvalidTargetObjectTypeException
   {
      if (ref == null)
         throw new IllegalArgumentException("Resource reference cannot be null.");

      // check that is a supported resource type
      // (concrete implementations need to implement this)
      if (!isSupportedResourceType(ref, resourceType))
         throw new InvalidTargetObjectTypeException("Unsupported resource type: " + resourceType);

      this.resource = ref;
      this.resourceType = resourceType;

      /*
      If the managed resource is a NotificationBroadcaster and does have a public method
      sendNotification on it, set our notifier to it. The sendNotification call
      will be invoked through reflection on the resource.
      This is currently the only way an xmbean can send notifications.
      */
      if(resource instanceof NotificationBroadcaster)
      {
         try
         {
            ReflectedNotificationBroadcaster temp = new ReflectedNotificationBroadcaster((NotificationBroadcaster)resource);
            notifier = temp;
         }
         catch (NoSuchMethodException ignore)
         {
            // no access to the sendNotification
            // so we keep the default notifier
         }
      }
   }

   // ModelMBeanNotificationBroadcaster implementation --------------
   public void addNotificationListener(NotificationListener listener,
         NotificationFilter filter,
         Object handback)
   {
      notifier.addNotificationListener(listener, filter, handback);
   }

   public void removeNotificationListener(NotificationListener listener)
         throws ListenerNotFoundException
   {
      notifier.removeNotificationListener(listener);
   }

   public void addAttributeChangeNotificationListener(NotificationListener listener,
         String attributeName, Object handback) throws MBeanException
   {
      AttributeChangeNotificationFilter filter = new AttributeChangeNotificationFilter();
      filter.enableAttribute(attributeName);
      notifier.addNotificationListener(listener, filter, handback);
   }

   public void removeAttributeChangeNotificationListener(NotificationListener listener, String attributeName)
         throws MBeanException, ListenerNotFoundException
   {
      notifier.removeNotificationListener(listener);
   }

   public void sendNotification(String message) throws MBeanException
   {
      Notification notif = new Notification(
            GENERIC_MODELMBEAN_NOTIFICATION, // type
            this, // source
            ++notifierSequence, // sequence number
            message                          // message
      );

      sendNotification(notif);
   }

   public void sendNotification(Notification notification)
         throws MBeanException
   {
      notifier.sendNotification(notification);
   }

   public void sendAttributeChangeNotification(AttributeChangeNotification notification)
         throws MBeanException
   {
      notifier.sendNotification(notification);
   }

   public void sendAttributeChangeNotification(Attribute oldValue, Attribute newValue)
         throws MBeanException
   {
      String attr = oldValue.getName();
      String type = getModelMBeanInfo().getAttribute(attr).getType();

      AttributeChangeNotification notif = new AttributeChangeNotification(
            this, // source
            ++attrNotifierSequence, // seq. #
            System.currentTimeMillis(), // time stamp
            "" + attr + " changed from " + oldValue + " to " + newValue,
            attr, type, // name & type
            oldValue.getValue(),
            newValue.getValue()            // values
      );

      notifier.sendNotification(notif);
   }

   public MBeanNotificationInfo[] getNotificationInfo()
   {
      // FIXME: NYI
      throw new Error("NYI");
   }

   // PersistentMBean implementation --------------------------------
   public void load() throws MBeanException, InstanceNotFoundException
   {
      if (getModelMBeanInfo() == null)
      {
         return;
      }
      persistence.load(this, (MBeanInfo) getModelMBeanInfo());
   }

   public void store() throws MBeanException, InstanceNotFoundException
   {
      persistence.store((MBeanInfo) getModelMBeanInfo());
   }

   // MBeanRegistration implementation ------------------------------
   /**
    * Describe <code>preRegister</code> method here.
    *
    * @param server a <code>MBeanServer</code> value
    * @param name an <code>ObjectName</code> value
    * @return an <code>ObjectName</code> value
    * @exception Exception if an error occurs
    *
    * @todo send the MBeanRegistration calls through the stack.
    * Some interceptors might like to know these lifecycle events.
    */
   public ObjectName preRegister(MBeanServer server, ObjectName name) throws Exception
   {
      ModelMBeanInfoSupport infoSupport = (ModelMBeanInfoSupport) getModelMBeanInfo();
      Descriptor mbeanDescriptor = infoSupport.getDescriptor(null, MBEAN_DESCRIPTOR);
      // See if there are any interceptors to insert at the front of the stack
      Descriptor[] interceptorDescriptors = (Descriptor[]) mbeanDescriptor.getFieldValue(INTERCEPTORS);
      Interceptor[] interceptors;
      int index = 0;
      if( interceptorDescriptors == null )
      {
         interceptors = new Interceptor[3];
         // Use the default interceptors
         interceptors[index ++] = new PersistenceInterceptor2(infoSupport, this);
         interceptors[index ++] = new MBeanAttributeInterceptor(infoSupport, this);
         interceptors[index ++] = new ObjectReferenceInterceptor(infoSupport, this);
      }
      else
      {
         Interceptor[] tmp = new Interceptor[interceptorDescriptors.length];
         createInterceptors(interceptorDescriptors, tmp);
         index = interceptorDescriptors.length;
         interceptors = tmp;
      }

      // Chain the interceptors together
      stack = interceptors[0];
      Interceptor lastInterceptor = stack;
      for(int i = 1; i < interceptors.length; i ++)
      {
         Interceptor next = interceptors[i];
         lastInterceptor = lastInterceptor.setNext(next);
      }

      // Populate the xmbean attributes from value descriptors
      setValuesFromMBeanInfo(infoSupport);

      // Initialize the persistence layer
      initPersistence(server);

      //if resource is an MBeanRegistration, forward the lifecycle calls to it.
      //Most likely these calls should go through the interceptor stack with some
      //kind of "administrative" marker.
      if (resource instanceof MBeanRegistration)
      {
         return ((MBeanRegistration) resource).preRegister(server, name);
      } // end of if ()
      else
      {
         return name;
      } // end of else
   }

   public void postRegister(Boolean registrationSuccessful)
   {
      if (resource instanceof MBeanRegistration)
      {
         ((MBeanRegistration) resource).postRegister(registrationSuccessful);
      } // end of if ()
   }

   public void preDeregister() throws Exception
   {
      if (resource instanceof MBeanRegistration)
      {
         ((MBeanRegistration) resource).preDeregister();
      } // end of if ()
   }

   public void postDeregister()
   {
      if (resource instanceof MBeanRegistration)
      {
         ((MBeanRegistration) resource).postDeregister();
      } // end of if ()
   }

   // Accessors ---------------------------------------------------

   public ModelMBeanInfo getModelMBeanInfo()
   {
      return (ModelMBeanInfo) info;
   }

   // Protected ---------------------------------------------------

   /** Set the xmbean attributes from the ModelMBeanInfo value descriptors.
    * @param metadata the xmbean metadata
    * @throws JMException thrown on any setAttribute failure
    */
   protected void setValuesFromMBeanInfo(ModelMBeanInfo metadata)
      throws JMException
   {
      MBeanAttributeInfo[] attrs = metadata.getAttributes();
      for (int i = 0; i < attrs.length; i++)
      {
         /// for each attribute, create a new Attribute object and add it to the collection
         ModelMBeanAttributeInfo attributeInfo = (ModelMBeanAttributeInfo)attrs[i];
         if( attributeInfo.isWritable() == true )
         {
            Descriptor attrDesc = attributeInfo.getDescriptor();
            Object name = attrDesc.getFieldValue(ModelMBeanConstants.NAME);
            Object value = attrDesc.getFieldValue(ModelMBeanConstants.VALUE);
            if( value != null )
            {
               log.debug("loading attribute  name: " + name + ", value: " + value);
               Attribute curAttribute = new Attribute(name.toString(), value);
               super.setAttribute(curAttribute);
            }
         }
      }
   }

   /**
    * initializes the persistence manager based on the info for this bean.
    * If this is successful, loads the bean from the persistence store.
    */
   protected void initPersistence(MBeanServer server)
         throws MBeanException, InstanceNotFoundException
   {
      Descriptor[] descriptors;
      try
      {
         descriptors = getModelMBeanInfo().getDescriptors(MBEAN_DESCRIPTOR);
      }
      catch (MBeanException e)
      {
         log.error("Failed to obtain MBEAN_DESCRIPTORs", e);
         return;
      }

      if (descriptors == null)
      {
         return;
      }
      String persistMgrName = null;
      for (int i = 0; ((i < descriptors.length) && (persistMgrName == null)); i++)
      {
         persistMgrName = (String) descriptors[i].getFieldValue(PERSISTENCE_MANAGER);
      }
      if (persistMgrName == null)
      {
         log.debug("No "+PERSISTENCE_MANAGER
            +" descriptor found, null persistence will be used");
         return;
      }

      try
      {
         persistence = (PersistenceManager) server.instantiate(persistMgrName);
         log.debug("Loaded persistence mgr: "+persistMgrName);
      }
      catch (Exception cause)
      {
         log.error("Unable to instantiate the persistence manager:"
            + persistMgrName, cause);
      }
      load();
   }

   protected void createInterceptors(Descriptor[] interceptorDescriptors,
      Interceptor[] interceptors)
      throws Exception
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      for(int d = 0; d < interceptorDescriptors.length; d ++)
      {
         Descriptor desc = interceptorDescriptors[d];
         String code = (String) desc.getFieldValue("code");
         Class interceptorClass = loader.loadClass(code);
         Interceptor interceptor = null;
         // Check for a ctor(MBeanInfo info, MBeanInvoker invoker)
         Class[] ctorSig = {MBeanInfo.class, MBeanInvoker.class};
         try
         {
            Constructor ctor = interceptorClass.getConstructor(ctorSig);
            Object[] ctorArgs = {info, this};
            interceptor = (Interceptor) ctor.newInstance(ctorArgs);
         }
         catch(Throwable t)
         {
            log.debug("Failed to invoke ctor(MBeanInfo, MBeanInvoker) for: "
                  +interceptorClass, t);
            // Try the default ctor
            interceptor = (Interceptor) interceptorClass.newInstance();
         }
         interceptors[d] = interceptor;

         // Apply any attributes
         String[] names = desc.getFieldNames();
         HashMap propertyMap = new HashMap();
         if( names.length > 1 )
         {
            BeanInfo beanInfo = Introspector.getBeanInfo(interceptorClass);
            PropertyDescriptor[] props = beanInfo.getPropertyDescriptors();
            for(int p = 0; p < props.length; p ++)
            {
               propertyMap.put(Introspector.decapitalize(props[p].getName()), props[p]);
            }
            // Map each attribute to the corresponding interceptor property
            for(int n = 0; n < names.length; n ++)
            {
               String name = names[n];
               if( name.equals("code") )
                  continue;
               String text = (String) desc.getFieldValue(name);
               PropertyDescriptor pd = (PropertyDescriptor) propertyMap.get(name);
               if( pd == null )
                  throw new IntrospectionException("No PropertyDescriptor for attribute:"+name);
               Method setter = pd.getWriteMethod();
               if( setter != null )
               {
                  Class ptype = pd.getPropertyType();
                  PropertyEditor editor = PropertyEditorManager.findEditor(ptype);
                  if( editor == null )
                     throw new IntrospectionException("Cannot convert string to interceptor attribute:"+name);
                  editor.setAsText(text);
                  Object args[] = {editor.getValue()};
                  setter.invoke(interceptor, args);
               }
            }
         }
      }
   }

   /**
    * This class wrap a NotificationBroadcaster instance and enables
    * to call a sendNotification method on it through reflection.
    * The instance muste have the method :<br/>
    * public void sendNotification(Notification notification) throws MBeanException
    */
   public static class ReflectedNotificationBroadcaster
      implements NotificationBroadcaster
   {

      private static final Logger log = Logger.getLogger(ReflectedNotificationBroadcaster.class);

      // Attributes ----------------------------------------------------

      private NotificationBroadcaster delegate;
      private Method sender;

      // Static --------------------------------------------------------

      /**
       * @param delegate the delegate
       */
      public static ReflectedNotificationBroadcaster with(NotificationBroadcasterSupport delegate)
      {
         try
         {
            return new ReflectedNotificationBroadcaster(delegate);
         }
         catch (NoSuchMethodException ignore)
         {
            // impossible
            throw new RuntimeException("impossible");
         }
      }

      // Constructors --------------------------------------------------

      /**
       * @param delegate the delegate
       * @throws NoSuchMethodException if the delegate does not have a sendNotification method on it
       */
      public ReflectedNotificationBroadcaster(NotificationBroadcaster delegate) throws NoSuchMethodException
      {
         this.delegate = delegate;
         sender = delegate.getClass().getMethod("sendNotification", new Class[]{Notification.class});
      }

      // Public --------------------------------------------------------

      public void sendNotification(Notification notification) throws MBeanException
      {
         try
         {
            sender.invoke(delegate, new Object[]{notification});
         }
         catch (IllegalAccessException ignore)
         {
            // not possible
            log.error("Cannot invoke sendNotification on the magaed resource", ignore);
         }
         catch (IllegalArgumentException ignore)
         {
            // not possible
            log.error("Cannot invoke sendNotification on the magaed resource", ignore);
         }
         catch (InvocationTargetException e)
         {
            Throwable cause = e.getTargetException();
            if (cause instanceof MBeanException)
            {
               // rethrow it
               throw (MBeanException)cause;
            }
            if (cause instanceof RuntimeException)
            {
               // rethrow it
               throw (RuntimeException)cause;
            }
         }
      }

      // NotificationBroadcaster implementation ------------------------

      public void addNotificationListener(NotificationListener listener,
                                          NotificationFilter filter,
                                          Object handback)
              throws IllegalArgumentException
      {
         delegate.addNotificationListener(listener, filter, handback);
      }

      public void removeNotificationListener(NotificationListener listener)
              throws ListenerNotFoundException
      {
         delegate.removeNotificationListener(listener);
      }

      public MBeanNotificationInfo[] getNotificationInfo()
      {
         return delegate.getNotificationInfo();
      }
   }
}
