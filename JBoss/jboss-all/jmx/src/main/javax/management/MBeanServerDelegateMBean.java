/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.management;

/**
 * Management interface of the MBean server delegate MBean.
 *
 * @see javax.management.MBeanServerDelegate
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 *   
 */
public interface MBeanServerDelegateMBean
{

   public String getMBeanServerId();

   public String getSpecificationName();

   public String getSpecificationVersion();

   public String getSpecificationVendor();

   public String getImplementationName();

   public String getImplementationVersion();

   public String getImplementationVendor();
}

