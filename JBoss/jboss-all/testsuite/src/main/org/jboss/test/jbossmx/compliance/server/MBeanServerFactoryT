/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.server;

import org.jboss.test.jbossmx.compliance.TestCase;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import java.util.List;

public class MBeanServerFactoryTestCase
   extends TestCase
{
   public MBeanServerFactoryTestCase(String s)
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
      // FIXME THS - flesh this out
   }

}
