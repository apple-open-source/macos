/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jmx.mbean;

import org.jboss.system.Service;
import org.jboss.system.ServiceMBeanSupport;

/**
 * TestMBClassLoader.java
 *
 *
 * Created: Sun Feb 17 20:08:31 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 * @jmx:mbean name="jboss.test:service=BrokenDeployer"
 * @jmx:interface extends="org.jboss.system.Service"
 */

public class TestMBClassLoader extends ServiceMBeanSupport implements TestMBClassLoaderMBean
{
   public TestMBClassLoader ()
   {
      
   }

   /**
    * Describe <code>getClassLoader</code> method here.
    *
    * @return a <code>String</code> value
    * @jmx:managed-operation
    */
   public String getClassLoader()
   {
      return this.getClass().getClassLoader().toString();
   }
   
}// TestMBClassLoader
