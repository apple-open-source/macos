package org.jboss.invocation;

import java.util.HashMap;
import java.util.Map;
import java.util.Collections;
import java.util.Iterator;
import java.lang.reflect.Method;

/** A method invocation statistics collection class.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class InvocationStatistics
{
   /** A HashMap<Method, TimeStatistic> of the method invocations */
   private Map methodStats;

   public long concurrentCalls = 0;
   public long maxConcurrentCalls = 0;
   public long lastResetTime = System.currentTimeMillis();

   public class TimeStatistic
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
      methodStats = Collections.synchronizedMap(new HashMap());
   }

   /** Update the TimeStatistic for the given method. This synchronizes on
    * m to ensure that the TimeStatistic for m is updated atomically.
    *
    * @param m the method to update the statistics for.
    * @param elapsed the elapsed time in milliseconds for the invocation.
    */
   public void updateStats(Method m, long elapsed)
   {
      synchronized( m )
      {
         TimeStatistic stat = (TimeStatistic) methodStats.get(m);
         if( stat == null )
         {
            stat = new TimeStatistic();
            methodStats.put(m, stat);
         }
         stat.count ++;
         stat.totalTime += elapsed;
         if( stat.minTime > elapsed )
            stat.minTime = elapsed;
         if( stat.maxTime < elapsed )
            stat.maxTime = elapsed;
      }
   }

   public synchronized void callIn ()
   {
      concurrentCalls++;
      if (concurrentCalls > maxConcurrentCalls)
         maxConcurrentCalls = concurrentCalls;
   }

   public synchronized void callOut ()
   {
      concurrentCalls--;
   }

   /** Resets all current TimeStatistics.
    *
    */
   public void resetStats()
   {
      synchronized( methodStats )
      {
         Iterator iter = methodStats.values().iterator();
         while( iter.hasNext() )
         {
            TimeStatistic stat = (TimeStatistic) iter.next();
            stat.reset();
         }
      }
      maxConcurrentCalls = 0;
      lastResetTime = System.currentTimeMillis();
   }

   /** Access the current collection of method invocation statistics
    *
    * @return A HashMap<Method, TimeStatistic> of the method invocations
    */
   public Map getStats()
   {
      return methodStats;
   }

   /** Generate an XML fragement for the InvocationStatistics. The format is
    * <InvocationStatistics concurrentCalls="c">
    *    <method name="aMethod" count="x" minTime="y" maxTime="z" totalTime="t" />
    *    ...
    * </InvocationStatistics>
    *
    * @return an XML representation of the InvocationStatistics
    */
   public String toString()
   {
      StringBuffer tmp = new StringBuffer("<InvocationStatistics concurrentCalls='");
      tmp.append(concurrentCalls);
      tmp.append("' >\n");

      HashMap copy = new HashMap(methodStats);
      Iterator iter = copy.entrySet().iterator();
      while( iter.hasNext() )
      {
         Map.Entry entry = (Map.Entry) iter.next();
         TimeStatistic stat = (TimeStatistic) entry.getValue();
         tmp.append("<method name='");
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
