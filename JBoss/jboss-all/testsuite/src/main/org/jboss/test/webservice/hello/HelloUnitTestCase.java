/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: HelloUnitTestCase.java,v 1.1.2.1 2003/09/29 23:46:50 starksm Exp $

package org.jboss.test.webservice.hello;

import org.jboss.net.axis.AxisInvocationHandler;

import org.jboss.test.webservice.AxisTestCase;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;


import java.net.URL;

/**
 * Tests remote accessibility of stateless ejb bean
 * @created 5. Oktober 2001, 12:11
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class HelloUnitTestCase extends AxisTestCase
{


   protected String HELLO_END_POINT = END_POINT + "/Hello";

   // Constructors --------------------------------------------------
   public HelloUnitTestCase(String name)
   {
      super(name);
   }

   /** the session bean with which we interact */
   Hello hello;

   /** setup the bean */
   public void setUp() throws Exception
   {
      super.setUp();
      hello = (Hello) createAxisService(Hello.class,
         new URL(HELLO_END_POINT));
   }

   /** where the config is stored */
   protected String getAxisConfiguration()
   {
      return "webservice/hello/client/" + super.getAxisConfiguration();
   }

   /** test a simple hello world */
   public void testHello() throws Exception
   {
      assertEquals("Return value must be hello.", hello.hello("World"), "Hello World!");
   }

   /** test some structural parameters */
   public void testHowdy() throws Exception
   {
      HelloData data = new HelloData();
      data.setName("CGJ");
      assertEquals("Return value must be howdy.", hello.howdy(data), "Howdy CGJ!");
   }

   /** this is to deploy the whole ear */
   public static Test suite() throws Exception
   {
      return getDeploySetup(HelloUnitTestCase.class, "wshello.ear");
   }

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(HelloUnitTestCase.class);
   }

}
