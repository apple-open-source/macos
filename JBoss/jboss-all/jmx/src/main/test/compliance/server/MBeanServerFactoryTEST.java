/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server;

import junit.framework.TestCase;
import junit.framework.AssertionFailedError;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;

import java.util.List;
import java.util.Iterator;


public class MBeanServerFactoryTEST extends TestCase
{
   public MBeanServerFactoryTEST(String s)
   {
      super(s);
   }

   public void testFindNonCreated()
   {
      MBeanServer server = MBeanServerFactory.newMBeanServer();
      List mbsList = MBeanServerFactory.findMBeanServer(null);
      assertEquals(0, mbsList.size());
   }

   public void testCreateFindAndRelease()
   {
      MBeanServer server = null;
      List mbsList = null;

      try
      {
         server = MBeanServerFactory.createMBeanServer();
         mbsList = MBeanServerFactory.findMBeanServer(null);
         assertEquals(1, mbsList.size());
      }
      finally
      {
         if (null != server)
         {
            MBeanServerFactory.releaseMBeanServer(server);
         }
      }

      mbsList = MBeanServerFactory.findMBeanServer(null);
      assertEquals(0, mbsList.size());
   }

   public void testRemoveNonCreated()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.newMBeanServer();
         MBeanServerFactory.releaseMBeanServer(server);
         fail("expected an IllegalArgumentException");
      }
      catch (IllegalArgumentException e)
      {
      }
      catch (Exception e)
      {
         fail("expected an IllegalArgumentException but got: " + e.getMessage());
      }
   }

   public void testDomainCreated()
   {
      String domain = "dOmAiN";
      MBeanServer server = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer(domain);
         assertEquals(domain, server.getDefaultDomain());
         List mbsList = MBeanServerFactory.findMBeanServer(null);
         assertEquals(server, mbsList.get(0));
         assertTrue("expected server reference equality", mbsList.get(0) == server);
      }
      finally
      {
         if (null != server)
         {
            MBeanServerFactory.releaseMBeanServer(server);
         }
      }
   }

   public void testDomainNonCreated()
   {
      String domain = "dOmAiN";
      MBeanServer server = MBeanServerFactory.newMBeanServer(domain);
      assertEquals(domain, server.getDefaultDomain());
   }

   public void testFindByAgentID()
   {
      try
      {
         MBeanServer server1 = MBeanServerFactory.createMBeanServer();
         MBeanServer server2 = MBeanServerFactory.createMBeanServer();
         MBeanServer server3 = MBeanServerFactory.newMBeanServer();
         ObjectName delegateName = new ObjectName("JMImplementation:type=MBeanServerDelegate");
         
         String agentID1 = (String)server1.getAttribute(delegateName, "MBeanServerId");
         String agentID2 = (String)server2.getAttribute(delegateName, "MBeanServerId");
         String agentID3 = (String)server3.getAttribute(delegateName, "MBeanServerId");
         
         assertTrue((MBeanServer)MBeanServerFactory.findMBeanServer(agentID1).get(0) == server1);
         assertTrue((MBeanServer)MBeanServerFactory.findMBeanServer(agentID2).get(0) == server2);
         assertTrue(MBeanServerFactory.findMBeanServer(agentID3).size() == 0);
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }

}
