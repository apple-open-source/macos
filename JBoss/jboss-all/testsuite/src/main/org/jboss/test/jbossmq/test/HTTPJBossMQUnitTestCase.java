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

import junit.framework.Test;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestSetup;

/**
 * JBossMQ tests over HTTP
 */
public class HTTPJBossMQUnitTestCase extends JBossMQUnitTest
{
   public HTTPJBossMQUnitTestCase(String name) throws Exception
   {
      super(name);
   }

   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();

      suite.addTest(new JBossTestSetup(new TestSuite(HTTPJBossMQUnitTestCase.class))
      {
         protected void setUp() throws Exception
         {
            this.getLog().info("HTTPJBossMQUnitTestCase started");
            JBossMQUnitTest.TOPIC_FACTORY = "HTTPConnectionFactory";
            JBossMQUnitTest.QUEUE_FACTORY = "HTTPConnectionFactory";
         }
         protected void tearDown() throws Exception
         {
            this.getLog().info("HTTPJBossMQUnitTestCase done");
         }
      });

      return suite;
   }

   static public void main(String[] args)
   {
      String newArgs[] = { "org.jboss.test.jbossmq.test.HTTPJBossMQUnitTestCase" };
      junit.swingui.TestRunner.main(newArgs);
   }
}
