
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.test;

import javax.management.ObjectName;

import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.eardeployment.a.interfaces.SessionA;
import org.jboss.test.jmx.eardeployment.a.interfaces.SessionAHome;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionB;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionBHome;


/**
 * EarDeploymentUnitTestCase.java
 *
 *
 * Created: Thu Feb 21 20:54:55 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class EarDeploymentUnitTestCase extends JBossTestCase 
{
   // Constants -----------------------------------------------------
   protected final static int INSTALLED = 0;
   protected final static int CONFIGURED = 1;
   protected final static int CREATED = 2;
   protected final static int RUNNING = 3;
   protected final static int FAILED = 4;
   protected final static int STOPPED = 5;
   protected final static int DESTROYED = 6;
   protected final static int NOTYETINSTALLED = 7;

   private ObjectName serviceControllerName;
   
   public EarDeploymentUnitTestCase(String name)
   {
      super(name);
      
      try
      {
         serviceControllerName =
            new ObjectName("jboss.system:service=ServiceController");
      }
      catch(Exception ignore)
      {
      }
   }

   /**
    * The <code>testEarSubpackageVisibility</code> method tests if the classes in
    * subpackages of an ear are visible to each other when ejb's are deployed.
    * SessionA and SessionB are in different jars, and each refer to the other.
    * 
    *
    * @exception Exception if an error occurs
    */
   public void testEarSubpackageVisibility() throws Exception
   {
      String testUrl = "eardeployment.ear";
      
      deploy(testUrl);
      
      try {
         SessionAHome aHome = (SessionAHome)getInitialContext().lookup("eardeployment/SessionA");
         SessionBHome bHome = (SessionBHome)getInitialContext().lookup("eardeployment/SessionB");
         SessionA a = aHome.create();
         SessionB b = bHome.create();
         assertTrue("a call b failed!", a.callB());
         assertTrue("b call a failed!", b.callA());
      }
      finally
      {
         undeploy(testUrl);
      }
   }

   public void testEarDepends() throws Exception
   {
      String testUrl = "eardepends.ear";
   
      ObjectName dependentAName =
         new ObjectName("jboss.j2ee:jndiName=test/DependentA,service=EJB");
      ObjectName dependentBName =
         new ObjectName("jboss.j2ee:jndiName=test/DependentB,service=EJB");
         
      ObjectName independentName =
         new ObjectName("jboss.j2ee:jndiName=test/Independent,service=EJB");
         
      ObjectName testName = new ObjectName("test:name=Test");
   
      if (removeService(dependentAName))
      {
         log.warn(dependentAName + " is registered, removed");
      }
   
      if (removeService(dependentBName))
      {
         log.warn(dependentBName + " is registered, removed");
      }
   
      deploy(testUrl);
      
      try
      {
         assertTrue(
            dependentAName + " is not registered",
            getServer().isRegistered(dependentAName));
         assertTrue(
            dependentBName + " is not registered",
            getServer().isRegistered(dependentBName));
            
         assertTrue(dependentAName + " invalid state", checkState(dependentAName, RUNNING));
         assertTrue(dependentBName + " invalid state", checkState(dependentBName, RUNNING));
         assertTrue(testName + " invalid state", checkState(testName, RUNNING));
         assertTrue(independentName + " invalid state", checkState(independentName, RUNNING));
      }
      finally
      {
         undeploy(testUrl);
      }
   
      try
      {
         assertTrue(
            dependentAName + " is registered",
            !getServer().isRegistered(dependentAName));
         assertTrue(
            dependentBName + " is registered",
            !getServer().isRegistered(dependentBName));
      }
      finally
      {
         removeService(dependentAName);
         removeService(dependentBName);
      }
   }

   protected boolean removeService(final ObjectName mbean) throws Exception
   {
      boolean isRegistered = getServer().isRegistered(mbean);
      if(isRegistered) {
         invoke(serviceControllerName,
            "remove",
            new Object[] {
               mbean
            },
            new String[] {
               ObjectName.class.getName()
            }
         );
      }
      
      return isRegistered;
   }

   protected boolean checkState(ObjectName mbean, int state) throws Exception
   {
      Integer mbeanState = (Integer)getServer().getAttribute(mbean, "State");
      return state == mbeanState.intValue();
   }

}// EarDeploymentUnitTestCase
