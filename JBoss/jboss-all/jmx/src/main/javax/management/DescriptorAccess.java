/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;


/**
 * This interface is used to gain access to descriptors of a JMX component.<p>
 *
 * ModelMBeans use this interface in ModelMBeanInfo classes.
 *
 * @see javax.management.Descriptor
 * @see javax.management.modelmbean.ModelMBean
 * @see javax.management.modelmbean.ModelMBeanInfo
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 *
 */
public interface DescriptorAccess
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * Retrieves the descriptor.
    *
    * @return the descriptor.
    */
   public Descriptor getDescriptor();


   /**
    * Sets the descriptor.
    *
    * @param inDescriptor the new descriptor.
    */
   public void setDescriptor(Descriptor inDescriptor);
}
