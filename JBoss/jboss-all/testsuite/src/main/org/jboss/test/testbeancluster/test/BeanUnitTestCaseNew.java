/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.test.testbeancluster.test;


import java.util.Date;

import javax.naming.Context;
import javax.naming.InitialContext;

import junit.framework.Test;

import org.jboss.test.testbeancluster.interfaces.StatelessSession;
import org.jboss.test.testbean.interfaces.StatelessSessionHome;
import org.jboss.test.testbeancluster.interfaces.StatefulSession;
import org.jboss.test.testbean.interfaces.StatefulSessionHome;

import org.jboss.test.JBossClusteredTestCase;
import org.jboss.test.testbeancluster.interfaces.NodeAnswer;

/**
 * Test SLSB for load-balancing behaviour
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>12 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class BeanUnitTestCaseNew extends JBossClusteredTestCase
{
   static boolean deployed = false;
   public static int test = 0;
   static Date startDate = new Date();
   
   protected final String namingFactory =
   System.getProperty(Context.INITIAL_CONTEXT_FACTORY);
   
   protected final String providerURL =
   System.getProperty(Context.PROVIDER_URL);
   
   public BeanUnitTestCaseNew (String name) {
      super(name);
   }

   public static Test suite() throws Exception
   {
      Test t1 = JBossClusteredTestCase.getDeploySetup(BeanUnitTestCaseNew.class, "testbeancluster.jar");
      return t1;
   }

   public void testStatelessBeanLoadBalancing() 
   throws Exception
   {       
      getLog().debug(++test+"- "+"Trying the context...");
      
      Context ctx = new InitialContext();
      getLog().debug("OK");
      
      ///*
      getLog().debug("");
      getLog().debug("Test Stateless Bean load-balancing");
      getLog().debug("==================================");
      getLog().debug("");
      getLog().debug(++test+"- "+"Looking up the home nextgen.StatelessSession...");
      StatelessSessionHome  statelessSessionHome =
      (StatelessSessionHome) ctx.lookup("nextgen.StatelessSession");
      if (statelessSessionHome!= null ) getLog().debug("ok");
         getLog().debug(++test+"- "+"Calling create on StatelessSessionHome...");
      StatelessSession statelessSession =
      (StatelessSession)statelessSessionHome.create();
      assertTrue("statelessSessionHome.create() != null", statelessSession != null);
      getLog().debug("ok");
      
      getLog().debug(++test+"- "+"Calling getEJBHome() on StatelessSession...");
      assertTrue("statelessSession.getEJBHome() != null", statelessSession.getEJBHome() != null);
      getLog().debug("ok");
      
      getLog().debug(++test+"- "+"Reseting the number of calls made on beans (making 2 calls)... ");
      for (int i=0; i<6; i++)
      {
         getLog().debug(++test+"- "+" Reseting number... ");
         statelessSession.resetNumberOfCalls ();         
      }
      
      getLog().debug(++test+"- "+"Now making 20 calls on this remote... ");
      for (int i=0; i<20; i++)
      {
         getLog().debug(++test+"- "+" Calling remote... ");
         statelessSession.makeCountedCall ();         
      }
      
      getLog().debug(++test+"- "+"Getting the number of calls that have been performed on each bean... ");
      long node1 = statelessSession.getCallCount();
      getLog().debug(++test+"- "+"One node has received: " + node1);
      
      long node2 = statelessSession.getCallCount();
      getLog().debug(++test+"- "+"The other node has received: " + node2);
      
      if (node1 == node2 &&
          node1 == 10)
      {
         getLog().debug(++test+"- "+"Test is ok.");
      }
      else
      {
         getLog().debug(++test+"- "+"Something wrong has happened! Calls seems not to have been load-balanced.");
         fail ("Calls have not been correctly load-balanced on the SLSB remote interface.");
      }
      
      statelessSession.remove();
      getLog().debug("ok");
   }            
   
   public void testStatefulBeanFailover() 
   throws Exception
   {       
      getLog().debug(++test+"- "+"Trying the context...");
      
      Context ctx = new InitialContext();
      getLog().debug("OK");
      
      ///*
      getLog().debug("");
      getLog().debug("Test Stateful Bean Failover");
      getLog().debug("==================================");
      getLog().debug("");
      getLog().debug(++test+"- "+"Looking up the home nextgen.StatefulSession...");
      StatefulSessionHome  statefulSessionHome =
      (StatefulSessionHome) ctx.lookup("nextgen.StatefulSession");
      if (statefulSessionHome!= null ) getLog().debug("ok");
         getLog().debug(++test+"- "+"Calling create on StatefulSessionHome...");
      StatefulSession statefulSession =
      (StatefulSession)statefulSessionHome.create("Bupple-Dupple");
      assertTrue("statefulSessionHome.create() != null", statefulSession != null);
      getLog().debug("ok");
      
      NodeAnswer node1 = statefulSession.getNodeState ();
      getLog ().debug (node1);
      
      // Now we switch to the other node, simulating a failure on node 1
      //
      System.setProperty ("JBossCluster-DoFail", "once");
      NodeAnswer node2 = statefulSession.getNodeState ();      
      getLog ().debug (node2);
      
      assertTrue ("No failover has occured!", !node1.nodeId.equals (node2.nodeId));
      
      assertTrue ("Value is not identical on replicated node", "Bupple-Dupple".equals (node1.answer) &&
                     node1.answer.equals (node2.answer) );

      // we change our name to see if it replicates to node 1
      //
      statefulSession.setName ("Changed");
      
      // now we travel back on node 1
      //
      System.setProperty ("JBossCluster-DoFail", "once");
      node1 = statefulSession.getNodeState ();
      getLog ().debug (node1);
      
      assertTrue ("No failover has occured!", !node1.nodeId.equals (node2.nodeId));
      
      assertTrue ("Value is not identical on replicated node", "Changed".equals (node1.answer) );      
      
      statefulSession.remove();
      getLog().debug("ok");
   }            

}
