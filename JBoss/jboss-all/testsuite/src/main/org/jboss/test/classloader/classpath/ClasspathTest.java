/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.classpath;

import org.jboss.system.ServiceMBeanSupport;

/** 
 * A simple service to test for the given resource.
 *
 * @author adrian@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ClasspathTest
   extends ServiceMBeanSupport
   implements ClasspathTestMBean
{
   public boolean findResource(String name)
   {
      ClassLoader cl = getClass().getClassLoader();
      return cl.getResource(name) != null;
   }
}
