package org.jboss.web.tomcat.statistics;

import java.io.Serializable;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/** A web context invocation statistics collection class.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.1.1 $
 */
public class InvocationStatistics implements Serializable
{
   /** The serial version ID */
   private static final long serialVersionUID = 9153807780893455734L;

   /** A HashMap<String, TimeStatistic> of the method invocations */
   private Map ctxStats;
   /** The number of concurrent request across all contexts */
   public volatile int concurrentCalls = 0;
   /** The maximum number of concurrent request across all contexts */
   public volatile int maxConcurrentCalls = 0;
   /** Time of the last resetStats call */
   public long lastResetTime = System.currentTimeMillis();

   public static class TimeStatistic
   {
      public long count;
      public long minTime = Long.MAX_VALUE;
      public long maxTime;
      public long totalTime;

      public void reset()
      {
         count = 0;
         minTime = Long.MAX_VALUE;
         maxTime = 0;
         totalTime = 0;
      }
   }

   public InvocationStatistics()
   {
      ctxStats = Collections.synchronizedMap(new HashMap());
   }

   /** Update the TimeStatistic for the given ctx. This does not synchronize
    * on the TimeStatistic so the results are an approximate values.
    *
    * @param ctx the method to update the statistics for.
    * @param elapsed the elapsed time in milliseconds for the invocation.
    */
   public void updateStats(String ctx, long elapsed)
   {
      TimeStatistic stat = (TimeStatistic) ctxStats.get(ctx);
      if( stat == null )
      {
         stat = new TimeStatistic();
         ctxStats.put(ctx, stat);
      }
      stat.count ++;
      stat.totalTime += elapsed;
      if( stat.minTime > elapsed )
         stat.minTime = elapsed;
      if( stat.maxTime < elapsed )
         stat.maxTime = elapsed;
   }

   public void callIn()
   {
      concurrentCalls ++;
      if (concurrentCalls > maxConcurrentCalls)
         maxConcurrentCalls = concurrentCalls;
   }

   public void callOut()
   {
      concurrentCalls --;
   }

   /** Resets all current TimeStatistics.
    *
    */
   public void resetStats()
   {
      synchronized( ctxStats )
      {
         Iterator iter = ctxStats.values().iterator();
         while( iter.hasNext() )
         {
            TimeStatistic stat = (TimeStatistic) iter.next();
            stat.reset();
         }
      }
      maxConcurrentCalls = 0;
      lastResetTime = System.currentTimeMillis();
   }

   /** Access the current collection of ctx invocation statistics
    *
    * @return A HashMap<String, TimeStatistic> of the ctx invocations
    */
   public Map getStats()
   {
      return ctxStats;
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer("(concurrentCalls: ");
      tmp.append(concurrentCalls);
      tmp.append(", maxConcurrentCalls: ");
      tmp.append(maxConcurrentCalls);

      HashMap copy = new HashMap(ctxStats);
      Iterator iter = copy.entrySet().iterator();
      while( iter.hasNext() )
      {
         Map.Entry entry = (Map.Entry) iter.next();
         TimeStatistic stat = (TimeStatistic) entry.getValue();
         tmp.append("[webCtx: ");
         tmp.append(entry.getKey());
         tmp.append(", count=");
         tmp.append(stat.count);
         tmp.append(", minTime=");
         tmp.append(stat.minTime);
         tmp.append(", maxTime=");
         tmp.append(stat.maxTime);
         tmp.append(", totalTime=");
         tmp.append(stat.totalTime);
         tmp.append("];");
      }
      tmp.append(")");
      return tmp.toString();
   }

   /** Generate an XML fragement for the InvocationStatistics. The format is
    * <InvocationStatistics concurrentCalls="c" maxConcurrentCalls="x">
    *    <webCtx name="ctx" count="x" minTime="y" maxTime="z" totalTime="t" />
    *    ...
    * </InvocationStatistics>
    *
    * @return an XML representation of the InvocationStatistics
    */
   public String toXML()
   {
      StringBuffer tmp = new StringBuffer("<InvocationStatistics concurrentCalls='");
      tmp.append(concurrentCalls);
      tmp.append("' maxConcurrentCalls='");
      tmp.append(maxConcurrentCalls);
      tmp.append("' >\n");

      HashMap copy = new HashMap(ctxStats);
      Iterator iter = copy.entrySet().iterator();
      while( iter.hasNext() )
      {
         Map.Entry entry = (Map.Entry) iter.next();
         TimeStatistic stat = (TimeStatistic) entry.getValue();
         tmp.append("<webCtx name='");
         tmp.append(entry.getKey());
         tmp.append("' count='");
         tmp.append(stat.count);
         tmp.append("' minTime='");
         tmp.append(stat.minTime);
         tmp.append("' maxTime='");
         tmp.append(stat.maxTime);
         tmp.append("' totalTime='");
         tmp.append(stat.totalTime);
         tmp.append("' />\n");
      }
      tmp.append("</InvocationStatistics>");
      return tmp.toString();
   }
}
