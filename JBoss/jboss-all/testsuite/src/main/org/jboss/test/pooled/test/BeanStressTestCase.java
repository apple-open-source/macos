/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.test.pooled.test;

import java.rmi.*;


import javax.naming.Context;
import javax.naming.InitialContext;
import javax.ejb.DuplicateKeyException;
import javax.ejb.Handle;
import javax.ejb.EJBMetaData;
import javax.ejb.EJBHome;
import javax.ejb.HomeHandle;
import javax.ejb.ObjectNotFoundException;

import java.util.Date;
import java.util.Properties;
import java.util.Collection;
import java.util.Iterator;
import java.util.Enumeration;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.pooled.interfaces.StatelessSessionHome;
import org.jboss.test.pooled.interfaces.StatelessSession;
import org.jboss.test.JBossTestCase;
import org.jboss.invocation.pooled.interfaces.PooledInvokerProxy;

/**
* Sample client for the jboss container.
*
* @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
* @version $Id: BeanStressTestCase.java,v 1.1.2.2 2003/09/14 23:34:14 ejort Exp $
*/
public class BeanStressTestCase 
   extends JBossTestCase 
{
   org.apache.log4j.Category log = getLog();
   
   static boolean deployed = false;
   static int test = 0;
   static Date startDate = new Date();
   public int NUM_THREADS = getThreadCount();
   public int iterations = getIterationCount();
   
   protected final String namingFactory =
   System.getProperty(Context.INITIAL_CONTEXT_FACTORY);
   
   protected final String providerURL =
   System.getProperty(Context.PROVIDER_URL);

   public BeanStressTestCase(String name) {
      super(name);
   }
   

   boolean failed = false;

   InitialContext ctx;

   private StatelessSession getSession() throws Exception
   {
      
      StatelessSessionHome home = (StatelessSessionHome)new InitialContext().lookup("nextgen.StatelessSession");
      return home.create();
   }


   private class NewProxy implements Runnable
   {
      StatelessSessionHome home;
      public NewProxy(StatelessSessionHome home)
      {
         this.home = home;
      }
      public void run()
      {
         for (int i = 0; i < iterations; i++)
         {
            try 
            {
               StatelessSession obj = home.create();
               obj.noop();
               
            } 
            catch(Throwable t)
            {
               //t.printStackTrace();
               incFailures();
            }
         }
      }
   }


   private class OldProxy implements Runnable
   {
      StatelessSession proxy;
      public OldProxy(StatelessSession proxy)
      {
         this.proxy = proxy;
      }
      public void run()
      {
         for (int i = 0; i < iterations; i++)
         {
            try 
            {
               proxy.noop();
               
            } 
            catch(Throwable t)
            {
               //t.printStackTrace();
               incFailures();
            }
         }
      }
   }


   protected int failures = 0;

   public void testNewProxy() throws Exception
   {
      ctx = new InitialContext();
      for (int i = 0; i < 2; i++)
      {
         runNewProxy("PooledStatelessSession");
         System.out.println("usedPooled: " + PooledInvokerProxy.usedPooled);
         PooledInvokerProxy.usedPooled = 0;
         runNewProxy("StatelessSession");
      }
   }
   /**
    * Creates proxy at every invocation.
    */
   protected void runNewProxy(String ejbname) throws Exception
   {
      failures = 0;
      System.out.println("------------------------");
      System.out.println("**** NewProxy" + ejbname + " ****");
      
      StatelessSessionHome home = (StatelessSessionHome)ctx.lookup(ejbname);
      Thread[] threads = new Thread[NUM_THREADS];
      for (int i = 0; i < NUM_THREADS; i++)
      {
         threads[i] = new Thread(new NewProxy(home));
      }
      long start = System.currentTimeMillis();
      for (int i = 0; i < NUM_THREADS; i++)
      {
         threads[i].start();
      }
      for (int i = 0; i < NUM_THREADS; i++)
      {
         threads[i].join();
      }
      System.out.println("time: " + (System.currentTimeMillis() - start));
      System.out.println("failures: " + failures);
   }

   public void testOldProxy() throws Exception
   {
      ctx = new InitialContext();
      for (int i = 0; i < 2; i++)
      {
         runOldProxy("PooledStatelessSession");
         System.out.println("usedPooled: " + PooledInvokerProxy.usedPooled);
         PooledInvokerProxy.usedPooled = 0;
         runOldProxy("StatelessSession");
      }
   }
   /**
    * Creates bean proxy before timing and shares it between threads
    */
   protected void runOldProxy(String ejbname) throws Exception
   {
      failures = 0;
      System.out.println("------------------------");
      System.out.println("**** OldProxy " + ejbname + " ****");
      
      StatelessSessionHome home = (StatelessSessionHome)ctx.lookup(ejbname);
      StatelessSession proxy = home.create();
      Thread[] threads = new Thread[NUM_THREADS];
      for (int i = 0; i < NUM_THREADS; i++)
      {
         threads[i] = new Thread(new OldProxy(proxy));
      }
      long start = System.currentTimeMillis();
      for (int i = 0; i < NUM_THREADS; i++)
      {
         threads[i].start();
      }
      for (int i = 0; i < NUM_THREADS; i++)
      {
         threads[i].join();
      }
      System.out.println("time: " + (System.currentTimeMillis() - start));
      System.out.println("failures: " + failures);
   }

   protected synchronized void incFailures()
   {
      failures++;
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(BeanStressTestCase.class, "pooled.jar");
   }

}
