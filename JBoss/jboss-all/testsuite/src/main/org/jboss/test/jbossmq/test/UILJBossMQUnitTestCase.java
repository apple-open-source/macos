/*
 * Copyright (c) 2000 Hiram Chirino <Cojonudo14@hotmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.jboss.test.jbossmq.test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;

import junit.framework.TestSuite;
import junit.framework.Test;


/**
 * JBossMQ tests over UIL
 */
public class UILJBossMQUnitTestCase
   extends JBossMQUnitTest
{
   public UILJBossMQUnitTestCase(String name) throws Exception
   {
      super(name);
   }
   
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      
      suite.addTest(new JBossTestSetup(new TestSuite(UILJBossMQUnitTestCase.class))
      {
         protected void setUp() throws Exception
         {
            this.getLog().info("UILJBossMQUnitTestCase started");
            JBossMQUnitTest.TOPIC_FACTORY = "UILConnectionFactory";
            JBossMQUnitTest.QUEUE_FACTORY = "UILConnectionFactory";
         }
         protected void tearDown() throws Exception
         {
            this.getLog().info("UILJBossMQUnitTestCase done");
         }
      });

      return suite;
   }

   static public void main( String []args )
   {
      String newArgs[] = { "org.jboss.test.jbossmq.test.UILJBossMQUnitTestCase" };
      junit.swingui.TestRunner.main(newArgs);
   }
}
