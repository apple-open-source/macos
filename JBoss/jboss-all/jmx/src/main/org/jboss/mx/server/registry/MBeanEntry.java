/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.server.registry;

import org.jboss.mx.server.ServerConstants;

import javax.management.DynamicMBean;
import javax.management.ObjectName;
import javax.management.MBeanRegistration;

import java.util.Map;

/**
 * info@todo this docs
 *
 * @see org.jboss.mx.server.registry.MBeanRegistry
 * @see org.jboss.mx.server.MBeanServerImpl
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.8 $
 */
public class MBeanEntry
   implements ServerConstants
{
   // Attributes ----------------------------------------------------

   /**
    * The registered object name of the mbean
    */
   private ObjectName objectName = null;

   /**
    * The class name of the mbean
    */
   private String resourceClassName = null;

   /**
    * The object used to invoke the mbean
    */
   private DynamicMBean invoker  = null;

   /**
    * The mbean registered
    */
   private Object resource  = null;

   /**
    * The context classloader of the mbean
    */
   private ClassLoader cl  = null;

   /**
    * The value map of the mbean
    */
   private Map valueMap  = null;

   // Constructors --------------------------------------------------

   /**
    * Construct a new mbean registration entry.
    *
    * @param objectName the name with which the mbean is registered
    * @param invoker the dynamic mbean used to invoke the mbean
    * @param object the mbean
    * @param valueMap any other information to include in the registration
    */
   public MBeanEntry(ObjectName objectName, DynamicMBean invoker, 
                     Object resource, Map valueMap)
   {
      this.objectName = objectName;
      this.invoker = invoker;
      this.resourceClassName = resource.getClass().getName();
      this.resource = resource;
      this.valueMap = valueMap;

      // Adrian: Unpack the classloader because this is used alot
      if (valueMap != null)
         this.cl = (ClassLoader) valueMap.get(CLASSLOADER);
   }

   // Public --------------------------------------------------------

   /**
    * Retrieve the object name with the mbean is registered.
    *
    * @return the object name
    */
   public ObjectName getObjectName()
   {
      return objectName;
   }

   /**
    * Retrieve the invoker for the mbean.
    *
    * @return the invoker
    */
   public DynamicMBean getMBean()
   {
      return invoker;
   }

   /**
    * Retrieve the class name for the mbean.
    *
    * @return the class name
    */
   public String getResourceClassName()
   {
      return resourceClassName;
   }

   /**
    * Retrieve the mbean.
    *
    * @return the mbean
    */
   public Object getResourceInstance()
   {
      return resource;
   }

   /**
    * Retrieve the context class loader with which to invoke the mbean.
    *
    * @return the class loader
    */
   public ClassLoader getClassLoader()
   {
      return cl;
   }

   /**
    * Retrieve a value from the map.
    *
    * @return key the key to value
    * @return the value or null if there is no entry
    */
   public Object getValue(String key)
   {
      if (valueMap != null)
         return valueMap.get(key);
      return null;
   }
}
