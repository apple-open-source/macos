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

/** Test of EJB call invocation overhead.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.7.2.2 $
 */
public class PerfStressTestCase extends JBossTestCase
{
   protected String CLIENT_SESSION = "perf.ClientSession";
   protected String CLIENT_ENTITY = "local/perfClientEntity";
   protected String PROBE = "perf.Probe";
   protected String PROBE_CMT = "perf.ProbeCMT";
   protected String TX_SESSION = "perf.TxSession";
   protected String ENTITY = "perfEntity";
   protected String ENTITY2 = "perfEntity2";

   int iterationCount;
   int beanCount;

   public PerfStressTestCase(String name)
   {
      super(name);
      iterationCount = getIterationCount();
      beanCount = getBeanCount();
   }
   
   public void testClientSession() throws Exception
   {
      getLog().debug("+++ testClientSession()");
      Object obj = getInitialContext().lookup(CLIENT_SESSION);
      obj = PortableRemoteObject.narrow(obj, SessionHome.class);
      SessionHome home = (SessionHome) obj;
      getLog().debug("Found SessionHome @ jndiName=ClientSession");
      Session bean = home.create(CLIENT_ENTITY);
      getLog().debug("Created ClientSession");
      
      try
      {
         bean.create(0, getBeanCount());
      }
      catch(javax.ejb.CreateException e)
      {
         getLog().debug("Exception while creating entities: ", e);
      }
      
      long start = System.currentTimeMillis();
      bean.read(0);
      bean.read(0, getBeanCount());
      bean.write(0);
      bean.write(0, getBeanCount());
      bean.remove(0, getBeanCount());
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug("Elapsed time = "+(elapsed / iterationCount));
   }

   public void testTimings() throws Exception
   {
      getLog().debug("+++ testTimings()");
      Object obj = getInitialContext().lookup(PROBE);
      obj = PortableRemoteObject.narrow(obj, ProbeHome.class);
      ProbeHome home = (ProbeHome) obj;
      getLog().debug("Found ProbeHome @ jndiName=Probe");
      Probe bean = home.create();
      getLog().debug("Created Probe");
      warmup(bean);
      noop(bean);
      ping(bean);
      echo(bean);
   }

   public void testTimingsCMT() throws Exception
   {
      getLog().debug("+++ testTimingsCMT()");
      Object obj = getInitialContext().lookup(PROBE_CMT);
      obj = PortableRemoteObject.narrow(obj, ProbeHome.class);
      ProbeHome home = (ProbeHome) obj;
      getLog().debug("Found ProbeHome @ jndiName=ProbeCMT");
      Probe bean = home.create();
      getLog().debug("Created ProbeCMT");
      warmup(bean);
      noop(bean);
      ping(bean);
      echo(bean);
   }

   public void testTxTimings() throws Exception
   {
      getLog().debug("+++ testTxTimings()");
      Object obj = getInitialContext().lookup(TX_SESSION);
      obj = PortableRemoteObject.narrow(obj, TxSessionHome.class);
      TxSessionHome home = (TxSessionHome) obj;
      getLog().debug("Found TxSession @ jndiName=TxSession");
      TxSession bean = home.create();
      getLog().debug("Created TxSession");
      txRequired(bean);
      txRequiresNew(bean);
      txSupports(bean);
      txNotSupported(bean);
      requiredToSupports(bean);
      requiredToMandatory(bean);
      requiredToRequiresNew(bean);
   }
   public void testFindByPrimaryKey() throws Exception
   {
      getLog().debug("+++ testFindByPrimaryKey()");
      Object obj = getInitialContext().lookup(ENTITY);
      obj = PortableRemoteObject.narrow(obj, EntityHome.class);
      EntityHome home = (EntityHome) obj;
      getLog().debug("Found EntityHome @ jndiName=Entity");
      EntityPK key = new EntityPK(0);
      Entity bean = null;

      getLog().debug("Running with " + iterationCount + " instances...");
      findByPrimaryKey(key, home);
   }
   public void testFindByPrimaryKey2() throws Exception
   {
      getLog().debug("+++ testFindByPrimaryKey2()");
      Object obj = getInitialContext().lookup(ENTITY2);
      obj = PortableRemoteObject.narrow(obj, Entity2Home.class);
      Entity2Home home = (Entity2Home) obj;
      getLog().debug("Found EntityHome @ jndiName=Entity");
      Entity2PK key = new Entity2PK(0, "String0", new Double(0));
      Entity bean = null;

      getLog().debug("Running with " + iterationCount + " instances...");
      findByPrimaryKey(key, home);
   }

   private void warmup(Probe bean) throws Exception
   {
      bean.noop();
      bean.ping("Ping");
      bean.echo("Echo");
   }

   private void noop(Probe bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" noop() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
         bean.noop();
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" noop() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/noop");
   }
   private void ping(Probe bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" ping(PING) invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
         bean.ping("PING");
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" ping() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/noop");
   }
   private void echo(Probe bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" echo(ECHO) invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.echo("ECHO");
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" echo() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/noop");
   }
   private void txRequired(TxSession bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" txRequired() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txRequired();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" txRequired() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/txRequired");
   }
   private void txRequiresNew(TxSession bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" txRequired() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txRequiresNew();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" txRequiresNew() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/txRequiresNew");
   }
   private void txSupports(TxSession bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" txSupports() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txSupports();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" txSupports() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/txSupports");
   }
   private void txNotSupported(TxSession bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" txNotSupported() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txNotSupported();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" txNotSupported() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/txNotSupported");
   }
   private void requiredToSupports(TxSession bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" requiredToSupports() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.requiredToSupports();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" requiredToSupports() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/requiredToSupports");
   }
   private void requiredToMandatory(TxSession bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" requiredToMandatory() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.requiredToMandatory();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" requiredToMandatory() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/requiredToMandatory");
   }
   private void requiredToRequiresNew(TxSession bean) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" requiredToRequiresNew() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.requiredToRequiresNew();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" requiredToRequiresNew() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/requiredToRequiresNew");
   }

   private void findByPrimaryKey(EntityPK key, EntityHome home) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" findByPrimaryKey(key="+key+") invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         Entity bean = home.findByPrimaryKey(key);
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" findByPrimaryKey() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/findByPrimaryKey");
   }
   private void findByPrimaryKey(Entity2PK key, Entity2Home home) throws Exception
   {
      getLog().debug("Starting "+iterationCount+" findByPrimaryKey(key="+key+") invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         Entity bean = home.findByPrimaryKey(key);
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      getLog().debug(iterationCount+" findByPrimaryKey() invocations = "+elapsed+" ms, "+(elapsed / iterationCount)+" ms/findByPrimaryKey");
   }

   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();  
      suite.addTest(new TestSuite(PerfStressTestCase.class));

      // Create an initializer for the test suite
      Setup wrapper = new Setup(suite, "perf.jar", false);
      return wrapper;
   }

}
