/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AxisTestServices.java,v 1.1.2.1 2003/09/29 23:46:49 starksm Exp $

package org.jboss.test.webservice;

import org.jboss.test.JBossTestServices;

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

/**
 * helper functions to deal with the JBoss integrated axis service
 * @created  15. Oktober 2001, 18:39
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 * Change History:
 * <ul>
 * <li> jung, 6.2.2002: adapted to new test structure </li>
 * </ul>
 */
public class AxisTestServices
   extends JBossTestServices
{
   /** where the beast is located */
   protected final static String axisServiceName = "jboss.net:service=Axis";

   /** Creates new AxisTestServices */
   public AxisTestServices(String name)
   {
      super(name);
   }

   /**
    * Gets the AxisServiceName
    */
   ObjectName getAxisServiceName() throws MalformedObjectNameException
   {
      return new ObjectName(axisServiceName);
   }

   /**
    * Deploy a an axis web service
    */
   public void deployAxis(String name) throws Exception
   {
      super.deploy(name);
   }

   /**
    * Undeploy a web service
    */
   public void undeployAxis(String name) throws Exception
   {
      super.undeploy(name);
   }

}
