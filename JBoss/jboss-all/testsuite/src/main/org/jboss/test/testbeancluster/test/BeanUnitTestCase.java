/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.test.testbeancluster.test;

import junit.framework.Test;

/**
* Sample client for the jboss container.
*
* @author <a href="mailto:marc.fleury@ejboss.org">Marc Fleury</a>
* @author <a href="mailto:hugo@hugopinto.com">Hugo Pinto</a>
* @version $Id: BeanUnitTestCase.java,v 1.3 2002/04/13 15:30:23 slaboure Exp $
*/
public class BeanUnitTestCase 
extends org.jboss.test.testbean.test.BeanUnitTestCase
{
   public BeanUnitTestCase(String name) {
      super(name);
   }

   public static Test suite() throws Exception
   {
      Test t1 = getDeploySetup(BeanUnitTestCase.class, "bmp.jar");
      Test t2 = getDeploySetup(t1, "testbeancluster.jar");
      Test t3 = getDeploySetup(t2, "testbean2.jar");
      return t3;
   }

}
