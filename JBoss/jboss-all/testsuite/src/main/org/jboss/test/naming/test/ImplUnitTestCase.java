/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;


import java.net.URL;
import java.net.URLClassLoader;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NameAlreadyBoundException;
import javax.naming.NamingException;

import junit.framework.TestCase;
import junit.framework.Test;
import junit.framework.TestSuite;
import junit.textui.TestRunner;

import org.apache.log4j.Logger;

/** Simple unit tests for the jndi implementation.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class ImplUnitTestCase extends TestCase
{
   static final Logger log = Logger.getLogger(ImplUnitTestCase.class);

   
   /**
    * Constructor for the SimpleUnitTestCase object
    *
    * @param name  Test name
    */
   public ImplUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * Tests that the second time you create a subcontext you get an exception.
    *
    * @exception Exception  Description of Exception
    */
   public void testCreateSubcontext() throws Exception
   {
      log.debug("+++ testCreateSubcontext");
      InitialContext ctx = getInitialContext();
      ctx.createSubcontext("foo");
      try
      {
         ctx.createSubcontext("foo");
         fail("Second createSubcontext(foo) did NOT fail");
      }
      catch (NameAlreadyBoundException e)
      {
         log.debug("Second createSubcontext(foo) failed as expected");
      }
      ctx.createSubcontext("foo/bar");
      ctx.unbind("foo/bar");
      ctx.unbind("foo");
   }

   /** Lookup a name to test basic connectivity and lookup of a known name
    *
    * @throws Exception
    */
   public void testLookup() throws Exception
   {
      log.debug("+++ testLookup");
      InitialContext ctx = getInitialContext();
      Object obj = ctx.lookup("");
      log.debug("lookup('') = "+obj);
   }

   public void testEncPerf() throws Exception
   {
      int count = Integer.getInteger("jbosstest.threadcount", 10).intValue();
      int iterations = Integer.getInteger("jbosstest.iterationcount", 1000).intValue();
      log.info("Creating "+count+"threads doing "+iterations+" iterations");
      InitialContext ctx = getInitialContext();
      URL[] empty = {};
      Thread[] testThreads = new Thread[count];
      for(int t = 0; t < count; t ++)
      {
         ClassLoader encLoader = URLClassLoader.newInstance(empty);
         Thread.currentThread().setContextClassLoader(encLoader);
         Runnable test = new ENCTester(ctx, iterations);
         Thread thr = new Thread(test, "Tester#"+t);
         thr.setContextClassLoader(encLoader);
         thr.start();
         testThreads[t] = thr;
      }

      for(int t = 0; t < count; t ++)
      {
         Thread thr = testThreads[t];
         thr.join();
      }
   }

   static InitialContext getInitialContext() throws NamingException
   {
      InitialContext ctx = new InitialContext();
      return new InitialContext();
   }

   private static class ENCTester implements Runnable
   {
      Context enc;
      int iterations;

      ENCTester(InitialContext ctx, int iterations) throws Exception
      {
         log.info("CL: "+Thread.currentThread().getContextClassLoader());
         this.iterations = iterations;
         enc = (Context) ctx.lookup("java:comp");
         enc = enc.createSubcontext("env");
         enc.bind("int", new Integer(1));
         enc.bind("double", new Double(1.234));
         enc.bind("string", "str");
         enc.bind("url", new URL("http://www.jboss.org"));
      }

      public void run()
      {
         try
         {
            InitialContext ctx =  new InitialContext();
            for(int i = 0; i < iterations; i ++)
            {
               Integer i1 = (Integer) enc.lookup("int");
               i1 = (Integer) ctx.lookup("java:comp/env/int");
               Double d = (Double) enc.lookup("double");
               d = (Double) ctx.lookup("java:comp/env/double");
               String s = (String) enc.lookup("string");
               s = (String) ctx.lookup("java:comp/env/string");
               URL u = (URL) enc.lookup("url");
               u = (URL) ctx.lookup("java:comp/env/url");
            }
         }
         catch(Exception e)
         {
            e.printStackTrace();
         }
      }
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new TestSuite(ImplUnitTestCase.class));

      // Create an initializer for the test suite
      NamingServerSetup wrapper = new NamingServerSetup(suite);
      return wrapper; 
   }

   /** Used to run the testcase from the command line
    *
    * @param args  The command line arguments
    */
   public static void main(String[] args)
   {
      TestRunner.run(ImplUnitTestCase.suite());
   }
}
