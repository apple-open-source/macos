/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.implementation.util;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;

import org.jboss.test.jbossmx.implementation.TestCase;

import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.util.AgentID;


public class AgentIDTestCase
   extends TestCase 
   implements ServerConstants
{
   public AgentIDTestCase(String s)
   {
      super(s);
   }

   public void testCreate()
   {
      String id1 = AgentID.create();
      String id2 = AgentID.create();
      
      assertTrue(!id1.equals(id2));
   }
   
   public void testGet()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         String id1 = (String)server.getAttribute(new ObjectName(MBEAN_SERVER_DELEGATE), "MBeanServerId");
         String id2 = AgentID.get(server);
         
         assertTrue(id1.equals(id2));
      }
      catch (Throwable t)
      {
         log.debug("failed", t);
         fail("Unexpected error: " + t.toString());
      }  
   }
   
}
