/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.perf.ejb;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.NumberFormat;
import javax.naming.InitialContext;

import org.jboss.test.perf.interfaces.PerfResult;
import org.jboss.test.perf.interfaces.Probe;
import org.jboss.test.perf.interfaces.ProbeHome;
import org.jboss.test.perf.interfaces.ProbeLocal;
import org.jboss.test.perf.interfaces.ProbeLocalHome;
import org.jboss.test.util.Debug;
import org.jboss.test.util.ejb.SessionSupport;

/** A session bean that tests intra-VM EJB call invocation overhead. The
runProbeTests method accepts the number of iterations and returns a simple
html report showing the output of each test run against the Probe session.
 
@author Scott.Stark@jboss.org
@version $Revision: 1.1.6.1 $
*/
public class PerfTestSessionBean extends SessionSupport 
{
   private static NumberFormat fmt = NumberFormat.getInstance();
   static
   {
      fmt.setMinimumFractionDigits(3);
      fmt.setMaximumFractionDigits(3);
   }

   /** Run the unit tests using Probe and return a report as a string
    */
   public PerfResult runProbeTests(int iterationCount)
   {
      StringBuffer results = new StringBuffer("<h1>runProbeTests("+iterationCount+")</h1><pre>\n");
      int testCount = 0;
      int failureCount = 0;

      PerfResult result = new PerfResult();
      try
      {
         testCount ++;
         testProbeTimings(iterationCount, results);
      }
      catch(Exception e)
      {
         failureCount ++;
         formatException(e, "testTimings", results);
         result.error = e;
      }
      results.append('\n');

      results.append("\nTotal tests: "+testCount);
      results.append("\nTotal failures: "+failureCount);
      results.append("\n</pre>");
      result.report = results.toString();
      return result;
   }
   /** Run the unit tests using ProbeLocal and return a report as a string
    */
   public PerfResult runProbeLocalTests(int iterationCount)
   {
      StringBuffer results = new StringBuffer("<h1>runProbeLocalTests("+iterationCount+")</h1><pre>\n");
      int testCount = 0;
      int failureCount = 0;

      PerfResult result = new PerfResult();
      try
      {
         testCount ++;
         testProbeLocalTimings(iterationCount, results);
      }
      catch(Exception e)
      {
         failureCount ++;
         formatException(e, "testTimings", results);
         result.error = e;
      }
      results.append('\n');

      results.append("\nTotal tests: "+testCount);
      results.append("\nTotal failures: "+failureCount);
      results.append("\n</pre>");
      result.report = results.toString();
      return result;
   }

   private void testProbeTimings(int iterationCount, StringBuffer results) throws Exception
   {
      results.append("\n+++ testTimings()\n");
      Object obj = new InitialContext().lookup("java:comp/env/ejb/ProbeHome");
      Class homeClass = obj.getClass();
      ProbeHome home = null;
      results.append("ProbeHome Proxy class info:\n");
      Debug.displayClassInfo(homeClass, results);
      results.append("Local ProbeHome.class info:\n");
      Debug.displayClassInfo(ProbeHome.class, results);
      home = (ProbeHome) obj;

      results.append("\nFound ProbeHome");
      Probe bean = home.create();
      results.append("\nCreated Probe");
      warmup(bean, results);
      noop(bean, iterationCount, results);
      ping(bean, iterationCount, results);
      echo(bean, iterationCount, results);
   }
   private void testProbeLocalTimings(int iterationCount, StringBuffer results) throws Exception
   {
      results.append("\n+++ testTimings()\n");
      Object obj = new InitialContext().lookup("java:comp/env/ejb/ProbeLocalHome");
      Class homeClass = obj.getClass();         
      ProbeLocalHome home = null;
      results.append("ProbeLocalHome Proxy class info:\n");
      Debug.displayClassInfo(homeClass, results);
      results.append("Local ProbeLocalHome.class info:\n");
      Debug.displayClassInfo(ProbeLocalHome.class, results);

      home = (ProbeLocalHome) obj;
      results.append("\nFound ProbeLocalHome");
      ProbeLocal bean = home.create();
      results.append("\nCreated ProbeLocal");
      warmup(bean, results);
      noop(bean, iterationCount, results);
      ping(bean, iterationCount, results);
      echo(bean, iterationCount, results);
   }

   private void warmup(Probe bean, StringBuffer results) throws Exception
   {
      bean.noop();
      bean.ping("Ping");
      bean.echo("Echo");
   }
   private void warmup(ProbeLocal bean, StringBuffer results) throws Exception
   {
      bean.noop();
      bean.ping("Ping");
      bean.echo("Echo");
   }
   private void noop(Probe bean, int iterationCount, StringBuffer results) throws Exception
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
   private void noop(ProbeLocal bean, int iterationCount, StringBuffer results) throws Exception
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

   private void ping(Probe bean, int iterationCount, StringBuffer results) throws Exception
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
   private void ping(ProbeLocal bean, int iterationCount, StringBuffer results) throws Exception
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

   private void echo(Probe bean, int iterationCount, StringBuffer results) throws Exception
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
   private void echo(ProbeLocal bean, int iterationCount, StringBuffer results) throws Exception
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

   private void formatException(Throwable t, String testName, StringBuffer results)
   {
      StringWriter sw = new StringWriter();
      PrintWriter pw = new PrintWriter(sw);
      t.printStackTrace(pw);
      results.append("\n"+testName+" failed:\n");
      results.append(sw.toString());
   }

}
