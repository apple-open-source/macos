/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.clazz;

import org.jboss.system.ServiceMBeanSupport;

/**
 * A simple service to test class loading.
 * 
 * @author claudio.vesco@previnet.it
 */
public class ClazzTest
   extends ServiceMBeanSupport
   implements ClazzTestMBean
{
   /* (non-Javadoc)
    * @see org.jboss.test.classloader.clazz.ClazzTestMBean#loadClass(java.lang.String)
    */
   public void loadClass(String clazz) throws Exception {
      ClassLoader cl = getClass().getClassLoader();
      
      cl.loadClass(clazz);
   }

   /* (non-Javadoc)
    * @see org.jboss.test.classloader.clazz.ClazzTestMBean#loadClassFromTCL(java.lang.String)
    */
   public void loadClassFromTCL(String clazz) throws Exception
   {
      ClassLoader tcl = Thread.currentThread().getContextClassLoader();
      
      tcl.loadClass(clazz);
   }
}
