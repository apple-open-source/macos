/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.util;

import junit.framework.TestCase;

import java.util.Set;

import java.lang.reflect.Method;

import javax.management.Attribute;
import javax.management.DynamicMBean;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.MBeanException;
import javax.management.InstanceNotFoundException;

import javax.management.modelmbean.RequiredModelMBean;

import test.implementation.util.support.Trivial;
import test.implementation.util.support.TrivialMBean;
import test.implementation.util.support.Trivial2;
import test.implementation.util.support.Trivial2MBean;

import org.jboss.mx.util.MBeanProxy;
import org.jboss.mx.util.ProxyContext;
import org.jboss.mx.util.AgentID;
import org.jboss.mx.util.DefaultExceptionHandler;

import test.implementation.util.support.MyInterface;
import test.implementation.util.support.Resource;

/**
 * Tests for mbean proxy
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.4 $ 
 */
public class MBeanProxyTEST extends TestCase
{
   public MBeanProxyTEST(String s)
   {
      super(s);
   }

   public void testGetWithServer() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:name=test");
   
      server.registerMBean(new Trivial(), oname);
   
      TrivialMBean mbean = (TrivialMBean)MBeanProxy.get(
            TrivialMBean.class, oname, server);      
            
      mbean.doOperation();
   }
   
   public void testGetWithAgentID() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      String agentID     = AgentID.get(server);
      ObjectName oname   = new ObjectName("test:name=test");
      
      server.registerMBean(new Trivial(), oname);

      TrivialMBean mbean = (TrivialMBean)MBeanProxy.get(
            TrivialMBean.class, oname, agentID);
            
      mbean.doOperation();
   }
   
   public void testCreateWithServer() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
      
      TrivialMBean mbean = (TrivialMBean)MBeanProxy.create(
            Trivial.class, TrivialMBean.class, oname, server);
            
      mbean.doOperation();
   }
   
   public void testCreateWithAgentID() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
      String agentID     = AgentID.get(server);
      
      TrivialMBean mbean = (TrivialMBean)MBeanProxy.create(
            Trivial.class, TrivialMBean.class, oname, agentID);
            
      mbean.doOperation();
   }
   
   public void testProxyInvocations() throws Exception
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

   public void testProxyInvocationWithConflictingMBeanAndContextMethods() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
      
      server.registerMBean(new Trivial(), oname);
      
      TrivialMBean mbean = (TrivialMBean)MBeanProxy.get(
            TrivialMBean.class, oname, AgentID.get(server));
            
      mbean.getMBeanServer();
      assertTrue(mbean.isGMSInvoked());
   }
   
   public void testContextAccess() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
      
      Trivial2MBean mbean = (Trivial2MBean)MBeanProxy.create(
            Trivial2.class, Trivial2MBean.class, oname, server
      );
    
      ProxyContext ctx = (ProxyContext)mbean;  
      
      ctx.getMBeanServer();
   }
   
   public void testProxyInvocationBetweenServers() throws Exception
   {
      MBeanServer server1 = MBeanServerFactory.createMBeanServer();
      MBeanServer server2 = MBeanServerFactory.createMBeanServer();
      ObjectName oname1   = new ObjectName("test:name=target");
      ObjectName oname2   = new ObjectName("test:name=proxy");
      
      // createMBean on server1 and retrieve a proxy to it
      Trivial2MBean mbean = (Trivial2MBean)MBeanProxy.create(
            Trivial2.class, Trivial2MBean.class, oname1, server1
      );
      
      //bind the proxy to server2
      server2.registerMBean(mbean, oname2);
      
      // invoke on server2
      server2.invoke(oname2, "doOperation", null, null);
      
      // check that server1 received the invocation
      assertTrue(((Boolean)server1.getAttribute(oname1, "OperationInvoked")).booleanValue());
   }
   
   public void testSimultaneousTypedAndDetypedInvocations() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
      
      Trivial2MBean mbean = (Trivial2MBean)MBeanProxy.create(
            Trivial2.class, Trivial2MBean.class, oname,server
      );
      
      // typed proxy interface
      mbean.setSomething("Kissa");
      assertTrue(mbean.getSomething().equals("Kissa"));
      
      // detyped proxy interface
      DynamicMBean mbean2 = (DynamicMBean)mbean;
      mbean2.setAttribute(new Attribute("Something", "Koira"));
      assertTrue(mbean2.getAttribute("Something").equals("Koira"));
      
      // direct local server invocation
      server.setAttribute(oname, new Attribute("Something", "Kissa"));
      assertTrue(server.getAttribute(oname, "Something").equals("Kissa"));
            
      // typed proxy interface invocation
      mbean.doOperation();
      assertTrue(mbean.isOperationInvoked());
      
      mbean.reset();
      
      // detyped proxy invocation
      mbean2.invoke("doOperation", null, null);
      assertTrue(((Boolean)mbean2.getAttribute("OperationInvoked")).booleanValue());
      
      mbean2.invoke("reset", null, null);
      
      // direct local server invocation
      server.invoke(oname, "doOperation", null, null);
      assertTrue(((Boolean)server.getAttribute(oname, "OperationInvoked")).booleanValue());
   }
   
   public void testContextAccessToMBeanServer() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
      
      Trivial2MBean mbean = (Trivial2MBean)MBeanProxy.create(
            Trivial2.class, Trivial2MBean.class, oname, server
      );
      
      // query the server this mbean is registered to
      ProxyContext ctx = (ProxyContext)mbean;
      MBeanServer srvr = ctx.getMBeanServer();
      
      Set mbeans = srvr.queryMBeans(new ObjectName("test:*"), null);
      ObjectInstance oi = (ObjectInstance)mbeans.iterator().next();
      
      assertTrue(oi.getObjectName().equals(oname));
      
      assertTrue(srvr.getAttribute(
            new ObjectName("JMImplementation:type=MBeanServerDelegate"),
            "ImplementationName"
      ).equals("JBossMX"));
      
   }
   
   public void testArbitraryInterfaceWithProxy() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
    
      RequiredModelMBean rmm = new RequiredModelMBean();
      Resource resource      = new Resource();
      
      rmm.setManagedResource(resource, "ObjectReference");
      rmm.setModelMBeanInfo(resource.getMBeanInfo());
    
      server.registerMBean(rmm, oname);
      
      MyInterface mbean  = (MyInterface)MBeanProxy.get(
            MyInterface.class, oname, server
      );
      
      mbean.setAttributeName("foo");
      mbean.setAttributeName2("bar");
      
      assertTrue(mbean.getAttributeName2().equals("bar"));
      assertTrue(mbean.doOperation().equals("tamppi"));
   }
   
   public void testCustomExceptionHandler() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      ObjectName oname   = new ObjectName("test:test=test");
      ObjectName oname2  = new ObjectName("test:test=test2");
      
      RequiredModelMBean rmm = new RequiredModelMBean();
      Resource resource      = new Resource();
      
      rmm.setManagedResource(resource, "ObjectReference");
      rmm.setModelMBeanInfo(resource.getMBeanInfo());
      
      server.registerMBean(rmm, oname);
      server.registerMBean(rmm, oname2);
      
      ProxyContext ctx  = (ProxyContext)MBeanProxy.get(
            MyInterface.class, oname, server
      );

      ctx.setExceptionHandler(new DefaultExceptionHandler() 
      {
         public Object handleInstanceNotFound(ProxyContext proxyCtx, InstanceNotFoundException e, Method m, Object[] args) throws Exception
         {
            return proxyCtx.getMBeanServer().invoke(new ObjectName("test:test=test2"), m.getName(), args, null);
         }
      });
         
      server.unregisterMBean(oname);
      
      MyInterface mbean = (MyInterface)ctx;
      assertTrue(mbean.doOperation().equals("tamppi"));      
   }
}
