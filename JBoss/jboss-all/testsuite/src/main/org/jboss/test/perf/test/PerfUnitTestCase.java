
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.perf.test;

import java.io.IOException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.perf.interfaces.PerfResult;
import org.jboss.test.perf.interfaces.PerfTestSession;
import org.jboss.test.perf.interfaces.PerfTestSessionHome;
import org.jboss.test.perf.interfaces.Probe;
import org.jboss.test.perf.interfaces.ProbeHome;

import org.jboss.test.JBossTestCase;

/** Tests of the Probe session bean method call overhead inside
of the JBoss VM. This is performed using the PerfTestSession
wrapper.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.2.2.1 $
*/
public class PerfUnitTestCase extends JBossTestCase
{
   int iterationCount;
   
   public PerfUnitTestCase(String name)
   {
      super(name);
      iterationCount = getIterationCount();
   }

   public void testInVMCalls() throws Exception
   {
      getLog().debug("+++ testInVMCalls()");
      Object obj = getInitialContext().lookup("PerfTestSession");
      obj = PortableRemoteObject.narrow(obj, PerfTestSessionHome.class);
      PerfTestSessionHome home = (PerfTestSessionHome) obj;
      getLog().debug("Found PerfTestSessionHome @ jndiName=PerfTestSessionHome");
      PerfTestSession bean = home.create();
      getLog().debug("Created PerfTestSession");
      long start = System.currentTimeMillis();
      PerfResult result = bean.runProbeTests(iterationCount);
      String report = result.report;
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug("Elapsed time = "+(elapsed / iterationCount));
      getLog().info("The testInVMCalls report is:\n"+report);
      if( result.error != null )
         throw result.error;
   }

   public void testInVMLocalCalls() throws Exception
   {
      getLog().debug("+++ testInVMLocalCalls()");
      Object obj = getInitialContext().lookup("PerfTestSession");
      obj = PortableRemoteObject.narrow(obj, PerfTestSessionHome.class);
      PerfTestSessionHome home = (PerfTestSessionHome) obj;
      getLog().debug("Found PerfTestSessionHome @ jndiName=PerfTestSessionHome");
      PerfTestSession bean = home.create();
      getLog().debug("Created PerfTestSession");
      long start = System.currentTimeMillis();
      PerfResult result = bean.runProbeLocalTests(iterationCount);
      String report = result.report;
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug("Elapsed time = "+(elapsed / iterationCount));
      getLog().info("The testInVMLocalCalls report is:\n"+report);
      if( result.error != null )
         throw result.error;
   }

   public static Test suite() throws Exception
   {
      Test test = getDeploySetup(PerfUnitTestCase.class, "probe.jar");
      return test;
   }

}
