/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.test;

import javax.management.ObjectName;

import org.jboss.test.JBossTestCase;
import org.jboss.deployment.IncompleteDeploymentException;

/**
 * @author  <a href="mailto:corby@users.sourceforge.net">Corby Page</a>
 */
public class MBeanDependsOnConnectionManagerUnitTestCase extends JBossTestCase
{
   // Attributes ----------------------------------------------------
   ObjectName serviceControllerName;

   public MBeanDependsOnConnectionManagerUnitTestCase( String name )
   {
      super( name );
      try
      {
         serviceControllerName = new ObjectName( "jboss.system:service=ServiceController" );
      }
      catch ( Exception e )
      {
      } // end of try-catch
   }

   public void testMBeanDependsOnConnectionManager() throws Exception
   {
      String mBeanCodeUrl = "testdeploy.sar";
      String mBeanUrl = "testmbeandependsOnConnectionManager-service.xml";
      String connectionManagerUrl = "hsqldb-singleconnection-ds.xml";

      ObjectName objectNameMBean = new ObjectName( "test:name=TestMBeanDependsOnConnectionManager" );
      ObjectName objectNameConnectionManager = new ObjectName( "jboss.jca:service=LocalTxCM,name=SingleConnectionDS" );

      deploy( mBeanCodeUrl );
      try
      {
         deploy( connectionManagerUrl );
         try
         {
            deploy( mBeanUrl );
            try
            {

               try
               {
                  undeploy( connectionManagerUrl );
                  deploy( connectionManagerUrl );
               }
               catch ( IncompleteDeploymentException ex )
               {
                  getLog().info("incomplete deployment exception", ex);
                  fail( "Connection Pool could not be recycled successfully!" );
               }

               // Double-check state
               String mBeanState = (String)getServer().getAttribute( objectNameMBean, "StateString" );
               assertEquals( "Test MBean not started!", "Started", mBeanState );
               String connectionManagerState = (String)getServer().getAttribute(
                  objectNameConnectionManager, "StateString" );
               assertEquals( "Connnection Manager MBean not started!", "Started", connectionManagerState );
            }
            finally
            {
               undeploy( mBeanUrl );
            }
         }
         finally
         {
            undeploy( connectionManagerUrl );
         }
      }
      finally
      {
         undeploy( mBeanCodeUrl );
      }
   }
}
