/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AxisUnitTestCase.java,v 1.1.2.1 2003/09/29 23:46:50 starksm Exp $

package org.jboss.test.webservice.axis;

import org.jboss.net.axis.AxisInvocationHandler;

import org.jboss.test.webservice.AxisTestCase;

import junit.framework.Test;
import junit.framework.TestSuite;

import java.net.URL;
import java.net.URLConnection;

import java.io.BufferedReader;
import java.io.InputStreamReader;

/**
 * Tests remote accessibility of the main axis service servlet
 * @created 11. Oktober 2001
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class AxisUnitTestCase extends AxisTestCase
{

   // Constructors --------------------------------------------------
   public AxisUnitTestCase(String name)
   {
      super(name);
   }

   /** tests a successful http call to the given target */
   protected void doCall(String message, URL target) throws Exception
   {
      BufferedReader reader =
         new BufferedReader(new InputStreamReader(target.openStream()));
      try
      {
         String line = reader.readLine();
         assertNotNull(message + " proper response", line);
      }
      finally
      {
         reader.close();
      }
   }

   /** tests availability of the servlet under various contexts */
   public void testServlet() throws Exception
   {
      doCall("Servlet availability", new URL(PROTOCOL + AXIS_CONTEXT + "servlet/AxisServlet"));
      doCall("service availability", new URL(END_POINT));
      doCall("list command", new URL(PROTOCOL + AXIS_CONTEXT + "servlet/AxisServlet?list"));
      doCall("service list ", new URL(END_POINT + "?list"));
   }

   public static Test suite() throws Exception
   {
      // nothing to deploy really
      return new TestSuite(AxisUnitTestCase.class);
   }
}