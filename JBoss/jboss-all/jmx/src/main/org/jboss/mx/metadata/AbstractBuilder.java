/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.metadata;

import java.util.Map;
import java.util.HashMap;

import javax.management.MBeanInfo;
import javax.management.NotCompliantMBeanException;

/**
 * Abstract helper class for builder implementations. Includes accessors
 * for property values that can deal with either string values or equivalent
 * object types (such as string <tt>"true"</tt> or <tt>Boolean(true)</tt>).
 *
 * @see org.jboss.mx.metadata.MetaDataBuilder
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public abstract class AbstractBuilder
   implements MetaDataBuilder
{

   // Attributes ----------------------------------------------------
   
   /**
    * Configuration properties.
    */
   protected Map properties = new HashMap();
   
   // Constructors --------------------------------------------------
   
   /**
    * Default constructor.
    */
   public AbstractBuilder() {}
   
   public AbstractBuilder(Map properties)
   {
      this.properties = properties;
   }
   
   // Public --------------------------------------------------------
   
   /**
    * Returns true for <tt>Boolean(true)</tt> and strings <tt>"true"</tt>
    * and <tt>"T"</tt> (case insensitive). Returns false for
    * <tt>Boolean(false)</tt> and strings <tt>"false"</tt> and <tt>"F"</tt>.
    *
    * @param   key to lookup
    *
    * @return true or false
    *
    * @throws IllegalPropertyException if property value is not either
    *         <tt>Boolean</tt> or <tt>String</tt> type or they key value is
    *         <tt>null</tt> or a string contained an unknown value
    */
   public boolean getBooleanProperty(String key) throws IllegalPropertyException
   {
      Object value = properties.get(key);
      
      if (value == null)
         throw new IllegalPropertyException("boolean property " + key + " does not exist");
         
      if (value instanceof String)
      {
         String v = (String) value;
         if (v.equalsIgnoreCase("true"))
            return true;
         if (v.equalsIgnoreCase("false"))
            return false;
         if (v.equalsIgnoreCase("t"))
            return true;
         if (v.equalsIgnoreCase("f"))
            return false;
            
         throw new IllegalPropertyException("unknown string value '" + v + "' for boolean property");
      }
      if (value instanceof Boolean)
         return ((Boolean)value).booleanValue();
         
      throw new IllegalPropertyException("illegal property type: " + value.getClass().getName());
   }
   
   /**
    * Returns a string property or <tt>null</tt> if key does not exist.
    */
   public String getStringProperty(String key)
   {
      return (String)properties.get(key);
   }
   
   
   // Implements MetaDataBuilder interface --------------------------
   
   /**
    * Sets a builder configuration property.
    */
   public void setProperty(String key, Object value)
   {
      properties.put(key, value);
   }
   
   /**
    * Returns the value of a given configuration property.
    */
   public Object getProperty(String key)
   {
      return properties.get(key);
   }

   public abstract MBeanInfo build() throws NotCompliantMBeanException;
   
   
   // Protected -----------------------------------------------------
   /**
    * Sets a copy of a properties map to this builder instance.
    *
    * @param   properties  configuration properties
    */
   protected void setProperties(Map properties)
   {
      this.properties = new HashMap(properties);
   }
   
   /**
    * Returns the property map of this builder instance.
    */
   protected Map getProperties()
   {
      return properties;
   }
   
   
}
      



