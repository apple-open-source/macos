/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: JmxUnitTestCase.java,v 1.1.2.1 2003/09/29 23:46:50 starksm Exp $

package org.jboss.test.webservice.jmx;

import org.jboss.net.axis.AxisInvocationHandler;
import org.jboss.net.jmx.MBeanInvocationHandler;
import org.jboss.net.jmx.adaptor.RemoteAdaptor;
import org.jboss.net.jmx.adaptor.RemoteAdaptorInvocationHandler;

import org.jboss.test.webservice.AxisTestCase;

import junit.framework.Test;
import junit.framework.TestSuite;

import javax.management.ObjectName;

import java.net.URL;

/**
 * Tests remote accessibility of JMX services
 * @created 11. Oktober 2001
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */
public class JmxUnitTestCase extends AxisTestCase
{

   protected String JMX_END_POINT = END_POINT + "/RemoteAdaptor";

   // static that holds the configured Axis jmx name
   protected static String AXIS_JMX_NAME = "jboss.net:service=Axis";

   // Constructors --------------------------------------------------
   public JmxUnitTestCase(String name)
   {
      super(name);
   }

   /** tests a very (untyped) basic call through the normal invocation handler */
   public void testBasic() throws Exception
   {
      log.info("+++ testBasic");
      AxisInvocationHandler handler = createAxisInvocationHandler(new URL(JMX_END_POINT));
      assertEquals("Testing basic invocation", "jboss",
         handler.invoke("RemoteAdaptor", "getDefaultDomain", new Object[0]));
      assertEquals("Testing complex invocation", Boolean.TRUE,
         handler.invoke("RemoteAdaptor", "isRegistered", new Object[]{
            new ObjectName(AXIS_JMX_NAME)})
      );
   }

   /** tests a very (untyped) basic call through the mbean invocation handler */
   public void testMBeanHandler() throws Exception
   {
      log.info("+++ testMBeanHandler");
      MBeanInvocationHandler handler =
         createMBeanInvocationHandler(new URL(JMX_END_POINT));
      assertEquals("Testing mbean specific invocation", "jboss",
         handler.invoke("RemoteAdaptor", "getDefaultDomain",
            new Object[0], new Class[0]));
      assertEquals("Testing custom serializer", Boolean.TRUE, handler.
         invoke("RemoteAdaptor", "isRegistered",
            new Object[]{new ObjectName(AXIS_JMX_NAME)},
            new Class[]{ObjectName.class}));
   }

   /** tests the (typed) adaptor access */
   public void testAdaptor() throws Exception
   {
      log.info("+++ testAdaptor");
      RemoteAdaptor handler = createRemoteAdaptor(new URL(JMX_END_POINT));
      assertEquals("Testing handler", "jboss",
         handler.getDefaultDomain());
      assertTrue("Testing handler with custom serializer", handler.
         isRegistered(new ObjectName(AXIS_JMX_NAME)));
   }

   public static Test suite() throws Exception
   {
      return new TestSuite(JmxUnitTestCase.class);
   }
}