/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.jmsra.test;

import javax.jms.*;
import javax.naming.InitialContext;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;

import org.jboss.test.jmsra.bean.*;

import junit.framework.Test;

/**
 * Test for jmsra.
 *
 * @author <a href="mailto:Adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */

public class RaJMSSessionUnitTestCase extends JBossTestCase
{
   public RaJMSSessionUnitTestCase(String name)
   {
      super(name);
   }

   public void testSendToQueueAndTopic()
      throws Exception
   {
      JMSSessionHome home = (JMSSessionHome) getInitialContext().lookup("JMSSession");
      JMSSession session = home.create();
      session.sendToQueueAndTopic();
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(RaJMSSessionUnitTestCase.class, "jmsra.jar");
   }
}





