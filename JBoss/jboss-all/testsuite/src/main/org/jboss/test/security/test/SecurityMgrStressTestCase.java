/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.test;

import java.util.HashSet;
import javax.security.auth.login.Configuration;
import javax.security.auth.Subject;

import junit.framework.TestCase;
import junit.textui.TestRunner;

import org.apache.log4j.Logger;
import org.jboss.security.plugins.JaasSecurityManager;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.auth.callback.SecurityAssociationHandler;
import org.jboss.util.TimedCachePolicy;

/** Stress testing of the JaasSecurityManager
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class SecurityMgrStressTestCase extends TestCase
{
   static final Logger log = Logger.getLogger(SecurityMgrStressTestCase.class);
   
   /**
    * Constructor for the SimpleUnitTestCase object
    *
    * @param name  Test name
    */
   public SecurityMgrStressTestCase(String name)
   {
      super(name);
   }

   /** Test concurrent access to the isValid and doesUserHaveRole security
    * mgr methods.
    *
    * @exception Exception thrown on any failure
    */
   public void testMTAuthentication() throws Exception
   {
      SecurityAssociation.setServer();
      int count = Integer.getInteger("jbosstest.threadcount", 10).intValue();
      int iterations = Integer.getInteger("jbosstest.iterationcount", 5000).intValue();
      log.info("Creating "+count+" threads doing "+iterations+" iterations");
      JaasSecurityManager secMgr = new JaasSecurityManager("testIdentity", new SecurityAssociationHandler());
      TimedCachePolicy cache = new TimedCachePolicy(3, false, 100);
      cache.create();
      cache.start();
      secMgr.setCachePolicy(cache);
      Thread[] testThreads = new Thread[count];
      AuthTester[] testers = new AuthTester[count];
      for(int t = 0; t < count; t ++)
      {
         AuthTester test = new AuthTester(secMgr, iterations);
         testers[t] = test;
         Thread thr = new Thread(test, "Tester#"+t);
         thr.start();
         testThreads[t] = thr;
      }

      for(int t = 0; t < count; t ++)
      {
         Thread thr = testThreads[t];
         thr.join();
         AuthTester test = testers[t];
         if( test.error != null )
            fail("Unexpected error seen by : "+test);
      }
   }

   protected void setUp()
   {
      // Install the custom JAAS configuration
      Configuration.setConfiguration(new LoginModulesUnitTestCase.TestConfig());
   }

   /** Used to run the testcase from the command line
    *
    * @param args  The command line arguments
    */
   public static void main(String[] args)
   {
      TestRunner.run(SecurityMgrStressTestCase.class);
   }

   private static class AuthTester implements Runnable
   {
      JaasSecurityManager secMgr;
      int iterations;
      Throwable error;

      AuthTester(JaasSecurityManager secMgr, int iterations) throws Exception
      {
         this.iterations = iterations;
         this.secMgr = secMgr;
      }

      public void run()
      {
         log.info("Begin run, t="+Thread.currentThread());
         SimplePrincipal user = new SimplePrincipal("stark");
         HashSet roleSet = new HashSet();
         roleSet.add(new SimplePrincipal("Java"));
         roleSet.add(new SimplePrincipal("Role3"));
         try
         {
            for(int i = 0; i < iterations; i ++)
            {
               Subject subject = new Subject();
               boolean authenticated = secMgr.isValid(user, "any", subject);
               if( authenticated == false )
                  throw new SecurityException("Failed to authenticate any");
               boolean authorized = secMgr.doesUserHaveRole(user, roleSet);
               if( authorized == false )
               {
                  Subject s = secMgr.getActiveSubject();
                  throw new SecurityException("Failed to authorize any, subject="+s);
               }
            }
         }
         catch(Throwable t)
         {
            error = t;
            log.error("Security failure", t);
         }
         log.info("End run, t="+Thread.currentThread());
      }
   }

}
