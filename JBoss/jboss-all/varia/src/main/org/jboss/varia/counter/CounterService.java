/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.counter;

import java.text.DecimalFormat;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Vector;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.Reference;
import javax.naming.StringRefAddr;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.naming.NonSerializableFactory;

/**
 * A service offering accumulator style counters to help in diagnosing
 * performance issues.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 * 
 * @author <a href="mailto:danch@nvisia.com">Dan Christopherson</href>
 * @version $Revision: 1.2 $
 */
public class CounterService
   extends ServiceMBeanSupport
   implements CounterServiceMBean
{
   public static final String JNDI_NAME = "java:/CounterService";

   private HashMap counterMap = new HashMap();
   
   /**
    * accumulate a count into a named counter. will initialize the named
    * counter if neccessary.
    */
   public void accumulate(String counterName, long add)
   {
      Counter counter = null;
      synchronized (counterMap)
      {
         counter = (Counter)counterMap.get(counterName);
         if (counter == null)
         {
            counter = new Counter(counterName);
            counterMap.put(counterName, counter);
         }
      }
      counter.addToCount(add);
   }
   
   protected void startService() throws Exception
   {
      InitialContext ctx = new InitialContext();
      
      //bind myself into JNDI, at java:/CounterService
      NonSerializableFactory.bind(JNDI_NAME, this);
      StringRefAddr addr = new StringRefAddr("nns", JNDI_NAME);
      Reference ref = new Reference(this.getClass().getName(), addr, NonSerializableFactory.class.getName(), null);
      ctx.bind(JNDI_NAME, ref);
   }
   
   protected void stopService() throws Exception
   {
      InitialContext ctx = new InitialContext();
      ctx.unbind(JNDI_NAME);
      NonSerializableFactory.unbind(JNDI_NAME);
   }
   
   /**
    * @jmx:managed-operation
    */
   public String list()
   {
      DecimalFormat format = new DecimalFormat("####0.0000");
      String retVal = "";
      Iterator keys = counterMap.keySet().iterator();
      while (keys.hasNext())
      {
         String key = (String)keys.next();
         Counter counter = (Counter)counterMap.get(key);
         long total = 0;
         int entries = 0;
         synchronized (counter)
         {//so we dont catch half of it.
            total = counter.getCount();
            entries = counter.getEntries();
         }
         double avg = ((double)total)/((double)entries);
         String descrip = key+": total="+total+" on "+entries+"entries for "+
         "an average of "+format.format(avg)+"<br>\n";
         retVal += descrip;
      }
      return retVal;
   }
   
   private static class Counter
   {
      private String name;
      private long count=0;
      private int entries=0;
      
      public Counter(String n)
      {
         name = n;
      }
      
      public String getName()
      {
         return name;
      }
      
      public synchronized long getCount()
      {
         return count;
      }
      
      public synchronized int getEntries()
      {
         return entries;
      }
      
      public synchronized void addToCount(long add)
      {
         count += add;
         entries++;
      }
   }
}
