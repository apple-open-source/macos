/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.mx.loading;

/** Just a tagging interface to make the UnifiedClassLoader so it can be
 * used as a loader in other MBean creation via MBeanServer.createMBean.
 *
 * @author <a href="scott.stark@jboss.org">Scott Stark</a>
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * 
 */
public interface UnifiedClassLoader3MBean extends UnifiedClassLoaderMBean
{
}
