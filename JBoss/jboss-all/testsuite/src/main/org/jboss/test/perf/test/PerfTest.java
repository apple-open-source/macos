/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.perf.test;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.CodeSource;
import java.security.ProtectionDomain;
import java.text.NumberFormat;
import javax.naming.InitialContext;

import org.jboss.test.perf.interfaces.Probe;
import org.jboss.test.perf.interfaces.ProbeHome;
import org.jboss.test.perf.interfaces.TxSession;
import org.jboss.test.perf.interfaces.TxSessionHome;

/** An MBean that tests intra-VM EJB call invocation overhead. The runTests
 *operation accepts the number of iterations and returns a simple html report
 *showing the output of each test run.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.4 $
 */
public class PerfTest implements PerfTestMBean
{
   private static NumberFormat fmt = NumberFormat.getInstance();
   static
   {
      fmt.setMinimumFractionDigits(3);
      fmt.setMaximumFractionDigits(3);
   }

   int iterationCount;

   public String runTests(int iterationCount)
   {
      StringBuffer results = new StringBuffer("<h1>PerfTest.results</h1><pre>\n");
      this.iterationCount = iterationCount;
      int testCount = 0;
      int failureCount = 0;

      // Print out some codebase info
      URL thisURL = getClass().getProtectionDomain().getCodeSource().getLocation();
      results.append("\nPertTest.ClassLoader="+getClass().getClassLoader());
      results.append("\nPertTest.codebase="+thisURL);
      try
      {
         testCount ++;
         testTimings(results);
      }
      catch(Throwable e)
      {
         failureCount ++;
         formatException(e, "testTimings", results);
      }
      results.append('\n');

      try
      {
         testCount ++;
         testTimingsCMT(results);
      }
      catch(Throwable e)
      {
         failureCount ++;
         formatException(e, "testTimingsCMT", results);
      }
      results.append('\n');

      try
      {
         testCount ++;
         testTxTimings(results);
      }
      catch(Throwable e)
      {
         failureCount ++;
         formatException(e, "testTxTimings", results);
      }
      results.append("\nTotal tests: "+testCount);
      results.append("\nTotal failures: "+failureCount);
      results.append("\n<pre>");

      return results.toString();
   }

   public void testTimings(StringBuffer results) throws Exception
   {
      results.append("\n+++ testTimings()");
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(ProbeHome.class.getClassLoader());
      try 
      {
         Object obj = new InitialContext().lookup("perf.Probe");
         // Print out some codebase info for the ProbeHome
         Class homeClass = obj.getClass();
         ClassLoader cl = homeClass.getClassLoader();
         results.append("\nProbeHome.ClassLoader="+cl);
         ClassLoader parent = cl;
         while( parent != null )
         {
            results.append("\n.."+parent);
            if( parent instanceof URLClassLoader )
            {
               URLClassLoader ucl = (URLClassLoader) parent;
               URL[] urls = ucl.getURLs();
               int length = urls != null ? urls.length : 0;
               for(int u = 0; u < length; u ++)
               {
                  results.append("\n...."+urls[u]);
               }
            }
            if( parent != null )
               parent = parent.getParent();
         }
         results.append("\nProbeHome Interfaces:");
         Class[] ifaces = homeClass.getInterfaces();
         for(int i = 0; i < ifaces.length; i ++)
         {
            results.append("\n++"+ifaces[i]);
            ProtectionDomain pd = ifaces[i].getProtectionDomain();
            CodeSource cs = pd.getCodeSource();
            if( cs != null )
               results.append("\n++++CodeSource: "+cs);
            else
               results.append("\n++++Null CodeSource");
         }
         CodeSource homeCS = ProbeHome.class.getProtectionDomain().getCodeSource();
         if( homeCS != null )
            results.append("\nPerfTest ProbHome CodeSource: "+homeCS);
         else
            results.append("\nPerfTest ProbHome CodeSource is NULL");
         
         ProbeHome home = (ProbeHome) obj;
         results.append("\n\nFound ProbeHome @ jndiName=Probe");
         Probe bean = home.create();
         results.append("\nCreated Probe");
         warmup(bean, results);
         noop(bean, results);
         ping(bean, results);
         echo(bean, results);
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCl);
      } // end of finally
   }

   public void testTimingsCMT(StringBuffer results) throws Exception
   {
      results.append("\n+++ testTimingsCMT()");
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(ProbeHome.class.getClassLoader());
      try 
      {
         Object obj = new InitialContext().lookup("perf.ProbeCMT");
         ProbeHome home = (ProbeHome) obj;
         results.append("\nFound ProbeHome @ jndiName=ProbeCMT");
         Probe bean = home.create();
         results.append("\nCreated ProbeCMT");
         warmup(bean, results);
         noop(bean, results);
         ping(bean, results);
         echo(bean, results);
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCl);
      } // end of finally
   }

   public void testTxTimings(StringBuffer results) throws Exception
   {
      results.append("\n+++ testTxTimings()");
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(TxSessionHome.class.getClassLoader());
      try 
      {
         Object obj = new InitialContext().lookup("perf.TxSession");
         TxSessionHome home = (TxSessionHome) obj;
         results.append("\nFound TxSession @ jndiName=TxSession");
         TxSession bean = home.create();
         results.append("\nCreated TxSession");
         txRequired(bean, results);
         txRequiresNew(bean, results);
         txSupports(bean, results);
         txNotSupported(bean, results);
         requiredToSupports(bean, results);
         requiredToMandatory(bean, results);
         requiredToRequiresNew(bean, results);
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCl);
      } // end of finally
   }

   private void warmup(Probe bean, StringBuffer results) throws Exception
   {
      bean.noop();
      bean.ping("Ping");
      bean.echo("Echo");
   }
   private void noop(Probe bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" noop() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
         bean.noop();
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" noop() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/noop");
   }
   private void ping(Probe bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" ping(PING) invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
         bean.ping("PING");
      long end = System.currentTimeMillis();
      long elapsed = end - start;      
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" ping() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/ping");
   }
   private void echo(Probe bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" echo(ECHO) invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.echo("ECHO");
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" echo() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/echo");
   }
   private void txRequired(TxSession bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" txRequired() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txRequired();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" txRequired() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/txRequired");
   }
   private void txRequiresNew(TxSession bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" txRequired() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txRequiresNew();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" txRequiresNew() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/txRequiresNew");
   }
   private void txSupports(TxSession bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" txSupports() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txSupports();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" txSupports() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/txSupports");
   }
   private void txNotSupported(TxSession bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" txNotSupported() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.txNotSupported();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" txNotSupported() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/txNotSupported");
   }
   private void requiredToSupports(TxSession bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" requiredToSupports() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.requiredToSupports();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" requiredToSupports() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/requiredToSupports");
   }
   private void requiredToMandatory(TxSession bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" requiredToMandatory() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.requiredToMandatory();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" requiredToMandatory() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/requiredToMandatory");
   }
   private void requiredToRequiresNew(TxSession bean, StringBuffer results) throws Exception
   {
      results.append("\nStarting "+iterationCount+" requiredToRequiresNew() invocations");
      long start = System.currentTimeMillis();
      for(int n = 0; n < iterationCount; n ++)
      {
         String echo = bean.requiredToRequiresNew();
      }
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      float avgTime = elapsed;
      avgTime /= iterationCount;
      results.append("\n"+iterationCount+" requiredToRequiresNew() invocations = "+elapsed+" ms, "
         + fmt.format(avgTime)+" ms/requiredToRequiresNew");
   }

   private void formatException(Throwable t, String testName, StringBuffer results)
   {
      StringWriter sw = new StringWriter();
      PrintWriter pw = new PrintWriter(sw);
      t.printStackTrace(pw);
      results.append("\n"+testName+" failed:\n");
      results.append(sw.toString());
   }

}
