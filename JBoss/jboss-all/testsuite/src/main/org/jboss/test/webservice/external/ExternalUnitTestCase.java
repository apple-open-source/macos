/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ExternalUnitTestCase.java,v 1.1.2.1 2003/09/29 23:46:50 starksm Exp $

package org.jboss.test.webservice.external;

import org.jboss.test.webservice.AxisTestCase;

import junit.framework.Test;

import javax.naming.InitialContext;

import java.net.URL;

/**
 * tests connectivity to external, global web services.
 * @version 	1.0
 * @author cgjung
 */

public class ExternalUnitTestCase extends AxisTestCase
{


   protected String FEDERATED_END_POINT = END_POINT + "/FederatedService";

   // Constructors --------------------------------------------------
   public ExternalUnitTestCase(String name)
   {
      super(name);
   }

   /** the session bean with which we interact */
   FederatedService federation;

   /** setup the bean */
   public void setUp() throws Exception
   {
      super.setUp();
      federation = (FederatedService) createAxisService(FederatedService.class, new URL(FEDERATED_END_POINT));
   }

   /** where the config is stored */
   protected String getAxisConfiguration()
   {
      return "webservice/external/client/" + super.getAxisConfiguration();
   }

   /** test a federated call */
   public void testFederated() throws Exception
   {
      String result = federation.findAndTranslate("JBoss is a killer server and Mr. Fleury is a damned genius");
      System.out.println("For your pleasure: " + result);
      assertEquals("got the translation right", "JBoss ist ein Mörderbediener und Herr Fleury ist ein verdammtes Genie ", result);
   }

   /** this is to deploy the whole ear */
   public static Test suite() throws Exception
   {
      return getDeploySetup(ExternalUnitTestCase.class, "wsexternal.ear");
   }

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(ExternalUnitTestCase.class);
   }
}