
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.missingclass;

import org.jboss.system.ServiceMBeanSupport;


/**
 * MissingClassTest.java
 *
 *
 * Created: Fri Aug  9 14:36:51 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 * @jmx.mbean name="jboss.test:name=missingclasstest" 
 *            extends="org.jboss.system.ServiceMBean"
 */

public class MissingClassTest 
   extends ServiceMBeanSupport
   implements MissingClassTestMBean
{
   public MissingClassTest (){
      
   }
}// MissingClassTest
