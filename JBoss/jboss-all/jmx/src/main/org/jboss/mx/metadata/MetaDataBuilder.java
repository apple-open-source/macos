/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.metadata;

import javax.management.MBeanInfo;
import javax.management.NotCompliantMBeanException;

/**
 * The <tt>MetaDataBuilder</tt> interface defines the contract between the
 * Model MBean and a metadata builder implementation. The metadata builder
 * implementations can extract the MBean management interface definition from
 * a given data source and construct the corresponding JMX MBeanInfo object
 * instances that define the Model MBean. <p>
 *
 * This interface also defines accessor methods for setting properties which
 * can be used to configure the builder implementations. See 
 * {@link #setProperty} and {@link #getProperty} methods for more information.
 *
 * @see     org.jboss.mx.metadata.AbstractBuilder
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3 $
 */
public interface MetaDataBuilder
{

   /**
    * Constructs the Model MBean metadata.
    *
    * @return initialized MBean info
    * @throws NotCompliantMBeanException if there were errors building the 
    *         MBean info from the given data source
    */
   public MBeanInfo build() throws NotCompliantMBeanException;

   /**
    * Sets a property that can be used to control the behaviour of the builder
    * implementation.
    *
    * @param   key      unique string key for a property
    * @param   value    property value
    */
   public void setProperty(String key, Object value);
   
   /**
    * Returns an existing property for this builder implementation.
    *
    * @param   key      property key string
    *
    * @return  property value
    */
   public Object getProperty(String key);
}

