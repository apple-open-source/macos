/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import java.rmi.RemoteException;
import javax.ejb.Handle;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;
import javax.jms.Message;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.LoginContext;

import org.jboss.security.auth.login.XMLLoginConfigImpl;
import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;
import org.jboss.test.security.interfaces.CalledSession;
import org.jboss.test.security.interfaces.CalledSessionHome;
import org.jboss.test.security.interfaces.StatefulSession;
import org.jboss.test.security.interfaces.StatefulSessionHome;
import org.jboss.test.security.interfaces.StatelessSession;
import org.jboss.test.security.interfaces.StatelessSessionHome;
import org.jboss.test.util.AppCallbackHandler;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestSuite;


/** Test of EJB spec conformace using the security-spec.jar
 deployment unit. These test the basic role based access model.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.14.2.8 $
 */
public class EJBSpecUnitTestCase
   extends JBossTestCase
{
   static String username = "scott";
   static char[] password = "echoman".toCharArray();
   static String QUEUE_FACTORY = "ConnectionFactory";
   
   LoginContext lc;
   boolean loggedIn;

   public EJBSpecUnitTestCase(String name)
   {
      super(name);
   }

   /** Test the use of getCallerPrincipal from within the ejbCreate
    * in a stateless session bean
    */
   public void testCreateCaller() throws Exception
   {
      log.debug("+++ testCreateCaller");
      login();
      InitialContext jndiContext = new InitialContext();
      Object obj = jndiContext.lookup("spec.SecureCreateSession");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found StatelessSessionHome");
      try
      {
         // The create should NOT be allowed to call getCallerPrincipal
         StatelessSession bean = home.create();
         // Need to invoke a method to ensure an ejbCreate call
         bean.noop();
         log.debug("Bean.noop(), ok");
         fail("Was able to call getCallerPrincipal in ejbCreate");
      }
      catch(Exception e)
      {
         log.debug("Create failed as expected", e);
      }
      logout();
   }

   /** Test the use of getCallerPrincipal from within the ejbCreate
    * in a stateful session bean
    */
   public void testStatefulCreateCaller() throws Exception
   {
      log.debug("+++ testStatefulCreateCaller");
      login();
      InitialContext jndiContext = new InitialContext();
      Object obj = jndiContext.lookup("spec.StatefulSession");
      obj = PortableRemoteObject.narrow(obj, StatefulSessionHome.class);
      StatefulSessionHome home = (StatefulSessionHome) obj;
      log.debug("Found StatefulSessionHome");
      // The create should be allowed to call getCallerPrincipal
      StatefulSession bean = home.create("testStatefulCreateCaller");
      // Need to invoke a method to ensure an ejbCreate call
      bean.echo("testStatefulCreateCaller");
      log.debug("Bean.echo(), ok");

      logout();
   }

   /** Test that:
    1. SecureBean returns a non-null principal when getCallerPrincipal
    is called with a security context and that this is propagated
    to its Entity bean ref.
    
    2. UnsecureBean throws an IllegalStateException when getCallerPrincipal
    is called without a security context.
    */
   public void testGetCallerPrincipal() throws Exception
   {
      logout();
      log.debug("+++ testGetCallerPrincipal()");
      Object obj = getInitialContext().lookup("spec.UnsecureStatelessSession2");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found Unsecure StatelessSessionHome");
      StatelessSession bean = home.create();
      log.debug("Created spec.UnsecureStatelessSession2");
      
      try
      {
         // This should fail because echo calls getCallerPrincipal()
         bean.echo("Hello from nobody?");
         fail("Was able to call StatelessSession.echo");
      }
      catch(RemoteException e)
      {
         log.debug("echo failed as expected");
      }
      bean.remove();
      
      login();
      obj = getInitialContext().lookup("spec.StatelessSession2");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      home = (StatelessSessionHome) obj;
      log.debug("Found spec.StatelessSession2");
      bean = home.create();
      log.debug("Created spec.StatelessSession2");
      // Test that the Entity bean sees username as its principal
      String echo = bean.echo(username);
      log.debug("bean.echo(username) = "+echo);
      assertTrue("username == echo", echo.equals(username));
      bean.remove();
   }
   
   /** Test that the calling principal is propagated across bean calls.
    */
   public void testPrincipalPropagation() throws Exception
   {
      log.debug("+++ testPrincipalPropagation");
      logout();
      login();
      Object obj = getInitialContext().lookup("spec.UnsecureStatelessSession2");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found Unsecure StatelessSessionHome");
      StatelessSession bean = home.create();
      log.debug("Created spec.UnsecureStatelessSession2");
      log.debug("Bean.forward('Hello') -> "+bean.forward("Hello"));
      bean.remove();
   }
   
   /** Test that the echo method is accessible by an Echo
    role. Since the noop() method of the StatelessSession
    bean was not assigned any permissions it should not be
    accessible by any user.
    */
   public void testMethodAccess() throws Exception
   {
      log.debug("+++ testMethodAccess");
      login();
      Object obj = getInitialContext().lookup("spec.StatelessSession");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found StatelessSessionHome");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSession");
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      
      try
      {
         // This should not be allowed
         bean.noop();
         fail("Was able to call StatelessSession.noop");
      }
      catch(RemoteException e)
      {
         log.debug("StatelessSession.noop failed as expected");
      }
      bean.remove();
   }

   /** Test that the echo method is accessible by an Echo
    role. Since the excluded() method of the StatelessSession
    bean has been placed into the excluded set it should not
    accessible by any user. This uses the security domain of the
    JaasSecurityDomain service to test its use as an authentication mgr.
    */
   public void testDomainMethodAccess() throws Exception
   {
      log.debug("+++ testDomainMethodAccess");
      login();
      Object obj = getInitialContext().lookup("spec.StatelessSessionInDomain");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found StatelessSessionInDomain home");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSessionInDomain");
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));

      try
      {
         // This should not be allowed
         bean.excluded();
         fail("Was able to call StatelessSession.excluded");
      }
      catch(RemoteException e)
      {
         log.debug("StatelessSession.excluded failed as expected");
      }
      bean.remove();
   }

   /** Test that the permissions assigned to the stateless session bean:
    with ejb-name=org/jboss/test/security/ejb/StatelessSession_test
    are read correctly.
    */
   public void testMethodAccess2() throws Exception
   {
      log.debug("+++ testMethodAccess2");
      login();
      InitialContext jndiContext = new InitialContext();
      Object obj = jndiContext.lookup("spec.StatelessSession_test");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found StatelessSessionHome");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSession_test");
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      bean.remove();
   }

   /** Test a user with Echo and EchoLocal roles can access the CalleeBean
    through its local interface by calling the CallerBean and that a user
    with only a EchoLocal cannot call the CallerBean.
    */
   public void testLocalMethodAccess() throws Exception
   {
      log.debug("+++ testLocalMethodAccess");
      login();
      InitialContext jndiContext = new InitialContext();
      Object obj = jndiContext.lookup("spec.CallerBean");
      obj = PortableRemoteObject.narrow(obj, CalledSessionHome.class);
      CalledSessionHome home = (CalledSessionHome) obj;
      log.debug("Found spec.CallerBean Home");
      CalledSession bean = home.create();
      log.debug("Created spec.CallerBean");
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      bean.remove();
   }

   /** Test access to a bean with a mix of remote interface permissions and
    * unchecked permissions with the unchecked permissions declared first.
    * @throws Exception
    */ 
   public void testUncheckedRemote() throws Exception
   {
      log.debug("+++ testUncheckedRemote");
      login();
      Object obj = getInitialContext().lookup("spec.UncheckedSessionRemoteLast");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found UncheckedSessionRemoteLast");
      StatelessSession bean = home.create();
      log.debug("Created spec.UncheckedSessionRemoteLast");
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      try
      {
         bean.excluded();
         fail("Was able to call UncheckedSessionRemoteLast.excluded");
      }
      catch(RemoteException e)
      {
         log.debug("UncheckedSessionRemoteLast.excluded failed as expected");         
      }
      bean.remove();
      logout();
   }

   /** Test access to a bean with a mix of remote interface permissions and
    * unchecked permissions with the unchecked permissions declared last.
    * @throws Exception
    */ 
   public void testRemoteUnchecked() throws Exception
   {
      log.debug("+++ testRemoteUnchecked");
      login();
      Object obj = getInitialContext().lookup("spec.UncheckedSessionRemoteFirst");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found UncheckedSessionRemoteFirst");
      StatelessSession bean = home.create();
      log.debug("Created spec.UncheckedSessionRemoteFirst");
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      try
      {
         bean.excluded();
         fail("Was able to call UncheckedSessionRemoteFirst.excluded");
      }
      catch(RemoteException e)
      {
         log.debug("UncheckedSessionRemoteFirst.excluded failed as expected");         
      }
      bean.remove();
      logout();
   }

   /** Test that a user with a role that has not been assigned any
    method permissions in the ejb-jar descriptor is able to access a
    method that has been marked as unchecked.
    */
   public void testUnchecked() throws Exception
   {
      log.debug("+++ testUnchecked");
      // Login as scott to create the bean
      login();
      Object obj = getInitialContext().lookup("spec.StatelessSession");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found spec.StatelessSession Home");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSession");
      // Logout and login back in as stark to test access to the unchecked method
      logout();
      login("stark", "javaman".toCharArray());
      bean.unchecked();
      log.debug("Called Bean.unchecked()");
      logout();
   }

   /** Test that a user with a valid role is able to access a
    bean for which all methods have been marked as unchecked.
    */
   public void testUncheckedWithLogin() throws Exception
   {
      log.debug("+++ testUncheckedWithLogin");
      // Login as scott to see that a user with roles is allowed access
      login();
      Object obj = getInitialContext().lookup("spec.UncheckedSession");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found spec.StatelessSession Home");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSession");
      bean.unchecked();
      log.debug("Called Bean.unchecked()");
      logout();
   }

   /** Test that user scott who has the Echo role is not able to
    access the StatelessSession2.excluded method even though
    the Echo role has been granted access to all methods of
    StatelessSession2 to test that the excluded-list takes
    precendence over the method-permissions.
    */
   public void testExcluded() throws Exception
   {
      log.debug("+++ testExcluded");
      login();
      Object obj = getInitialContext().lookup("spec.StatelessSession2");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found spec.StatelessSession2 Home");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSession2");
      try
      {
         bean.excluded();
         fail("Was able to call Bean.excluded()");
      }
      catch(Exception e)
      {
         log.debug("Bean.excluded() failed as expected");
         // This is what we expect
      }
      logout();
   }
   
   /** This method tests the following call chains:
    1. RunAsStatelessSession.echo() -> PrivateEntity.echo()
    2. RunAsStatelessSession.noop() -> RunAsStatelessSession.excluded()
    3. RunAsStatelessSession.forward() -> StatelessSession.echo()
    1. Should succeed because the run-as identity of RunAsStatelessSession
    is valid for accessing PrivateEntity.
    2. Should succeed ecause the run-as identity of RunAsStatelessSession
    is valid for accessing RunAsStatelessSession.excluded().
    3. Should fail because the run-as identity of RunAsStatelessSession
    is not Echo.
    */
   public void testRunAs() throws Exception
   {
      log.debug("+++ testRunAs");
      login();
      Object obj = getInitialContext().lookup("spec.RunAsStatelessSession");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found RunAsStatelessSession Home");
      StatelessSession bean = home.create();
      log.debug("Created spec.RunAsStatelessSession");
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      bean.noop();
      log.debug("Bean.noop(), ok");
      
      try
      {
         // This should not be allowed
         bean.forward("Hello");
         fail("Was able to call RunAsStatelessSession.forward");
      }
      catch(RemoteException e)
      {
         log.debug("StatelessSession.forward failed as expected");
      }
      bean.remove();
   }

   /** Test that an MDB with a run-as identity is able to access secure EJBs
    that require the identity.
    */
   public void testMDBRunAs() throws Exception
   {
      log.debug("+++ testMDBRunAs");
      logout();
      QueueConnectionFactory queueFactory = (QueueConnectionFactory) getInitialContext().lookup(QUEUE_FACTORY);
      Queue queA = (Queue) getInitialContext().lookup("queue/A");
      Queue queB = (Queue) getInitialContext().lookup("queue/B");
      QueueConnection queueConn = queueFactory.createQueueConnection();
      QueueSession session = queueConn.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Message msg = session.createMessage();
      msg.setStringProperty("arg", "HelloMDB");
      msg.setJMSReplyTo(queB);
      QueueSender sender = session.createSender(queA);
      sender.send(msg);
      sender.close();
      log.debug("Sent msg to queue/A");
      queueConn.start();
      QueueReceiver recv = session.createReceiver(queB);
      msg = recv.receive(5000);
      log.debug("Recv msg: "+msg);
      String info = msg.getStringProperty("reply");
      if( info.startsWith("Failed") )
      {
         fail("Recevied exception reply, info="+info);
      }
      recv.close();
      session.close();
      queueConn.close();
   }

   /** Test the security behavior of handles. To obtain secured bean from
      a handle that the handle be 
    */
   public void testHandle() throws Exception
   {
      log.debug("+++ testHandle");
      login();
      Object obj = getInitialContext().lookup("spec.StatelessSession");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found StatelessSessionHome");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSession");
      Handle h = bean.getHandle();
      log.debug("Obtained handle: "+h);
      bean = (StatelessSession) h.getEJBObject();
      log.debug("Obtained bean from handle: "+bean);
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      logout();

      /* Attempting to obtain the EJB fron the handle without security
       association present should fail
      */
      try
      {
         bean = (StatelessSession) h.getEJBObject();
         fail("Should not be able to obtain a bean without login info");
      }
      catch(Exception e)
      {
         log.debug("Obtaining bean from handle failed as expected, e="+e.getMessage());
      }

      // One should be able to obtain a handle without a login
      h = bean.getHandle();
      login();
      // Now we should be able to obtain and use the secure bean
      bean = (StatelessSession) h.getEJBObject();
      log.debug("Obtained bean from handle: "+bean);
      log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      logout();
   }

   /** Login as user scott using the conf.name login config or
    'spec-test' if conf.name is not defined.
    */
   private void login() throws Exception
   {
      login(username, password);
   }
   private void login(String username, char[] password) throws Exception
   {
      if( loggedIn )
         return;
      
      lc = null;
      String confName = System.getProperty("conf.name", "spec-test");
      AppCallbackHandler handler = new AppCallbackHandler(username, password);
      log.debug("Creating LoginContext("+confName+")");
      lc = new LoginContext(confName, handler);
      lc.login();
      log.debug("Created LoginContext, subject="+lc.getSubject());
      loggedIn = true;
   }
   private void logout() throws Exception
   {
      if( loggedIn )
      {
         loggedIn = false;
         lc.logout();
      }
   }


   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new TestSuite(EJBSpecUnitTestCase.class));

      // Create an initializer for the test suite
      TestSetup wrapper = new JBossTestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
            Configuration.setConfiguration(new XMLLoginConfigImpl());
            deploy("security-spec.jar");
            flushAuthCache();
         }
         protected void tearDown() throws Exception
         {
            undeploy("security-spec.jar");
            super.tearDown();
         
         }
      };
      return wrapper;
   }

}
