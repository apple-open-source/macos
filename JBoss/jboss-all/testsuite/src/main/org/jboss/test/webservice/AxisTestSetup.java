/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AxisTestSetup.java,v 1.1.2.1 2003/09/29 23:46:49 starksm Exp $

package org.jboss.test.webservice;

import org.jboss.test.JBossTestSetup;
import org.jboss.test.JBossTestServices;

import junit.framework.Test;

/**
 * Junit Test Setup decorator that is able to deploy web and J2ee services
 * @created 15. Oktober 2001, 18:33
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */
public class AxisTestSetup extends JBossTestSetup
{


   /** Creates new AxisSetup */
   public AxisTestSetup(Test test) throws Exception
   {
      super(test);
   }

   /** overrides services creation to install axis services */
   protected JBossTestServices createTestServices()
   {
      return new AxisTestServices(getClass().getName());
   }

   /**
    * Deploy a web service
    */
   protected void deployAxis(String name) throws Exception
   {
      ((AxisTestServices) delegate).deployAxis(name);
   }

   /**
    * Undeploy a web service
    */
   protected void undeployAxis(String name) throws Exception
   {
      ((AxisTestServices) delegate).undeployAxis(name);
   }
}