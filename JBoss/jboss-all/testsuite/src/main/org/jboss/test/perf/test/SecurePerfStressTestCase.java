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
import javax.security.auth.login.LoginContext;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.perf.interfaces.Entity;
import org.jboss.test.perf.interfaces.EntityPK;
import org.jboss.test.perf.interfaces.Entity2PK;
import org.jboss.test.perf.interfaces.EntityHome;
import org.jboss.test.perf.interfaces.Entity2Home;
import org.jboss.test.perf.interfaces.Probe;
import org.jboss.test.perf.interfaces.ProbeHome;
import org.jboss.test.perf.interfaces.Session;
import org.jboss.test.perf.interfaces.SessionHome;
import org.jboss.test.perf.interfaces.TxSession;
import org.jboss.test.perf.interfaces.TxSessionHome;

import org.jboss.test.JBossTestCase;

/** Test of EJB call invocation overhead in the presence of EJB security.
 * No more copied code!!
 *
 * @author Scott.Stark@jboss.org
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 *
 * @version $Revision: 1.5.2.2 $
 */
public class SecurePerfStressTestCase extends PerfStressTestCase //JBossTestCase
{
   {  // Override the PerfStressTestCase names
      CLIENT_SESSION = "secure/perf/ClientSession";
      CLIENT_ENTITY = "local/perfClientEntity";
      PROBE = "secure/perf/Probe";
      PROBE_CMT = "secure/perf/ProbeCMT";
      TX_SESSION = "secure/perf/TxSession";
      ENTITY = "secure/perf/Entity";
      ENTITY2 = "secure/perf/Entity2";
   }

   public SecurePerfStressTestCase(String name)
   {
      super(name);
   }

   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new TestSuite(SecurePerfStressTestCase.class));

      // Create an initializer for the test suite
      Setup wrapper = new Setup(suite, "secure-perf.jar", true);
      return wrapper;
   }
}
