/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.implementation.util;

import org.jboss.test.jbossmx.implementation.TestCase;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;

import org.jboss.test.jbossmx.implementation.util.support.Trivial;
import org.jboss.test.jbossmx.implementation.util.support.TrivialMBean;

import org.jboss.mx.util.MBeanProxy;
import org.jboss.mx.util.AgentID;


public class MBeanProxyTestCase
   extends TestCase
{
   public MBeanProxyTestCase(String s)
   {
      super(s);
   }

   public void testCreate()
   {
      try 
      {   
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName oname   = new ObjectName("test:name=test");
      
         server.registerMBean(new Trivial(), oname);
      
         TrivialMBean mbean = (TrivialMBean)MBeanProxy.get(
               TrivialMBean.class, oname, AgentID.get(server));      
      }
      catch (Throwable t)
      {
         log.debug("failed", t);
         fail("unexpected error: " + t.toString());
      }
   }

   public void testProxyInvocations()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName oname   = new ObjectName("test:name=test");
         
         server.registerMBean(new Trivial(), oname);
         
         TrivialMBean mbean = (TrivialMBean)MBeanProxy.get(
               TrivialMBean.class, oname, AgentID.get(server));
         
         mbean.doOperation();
         mbean.setSomething("JBossMX");
         
         assertEquals("JBossMX", mbean.getSomething());
      }
      catch (Throwable t)
      {
         log.debug("failed", t);
         fail("unexpected error: " + t.toString());
      }
   }
}
