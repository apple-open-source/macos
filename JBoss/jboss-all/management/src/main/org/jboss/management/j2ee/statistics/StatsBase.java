package org.jboss.management.j2ee.statistics;

import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.io.Serializable;
import javax.management.j2ee.statistics.Stats;
import javax.management.j2ee.statistics.Statistic;

/** The base JSR77.6.10 Stats interface base implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.4 $
 */
public class StatsBase
   implements Stats, Serializable
{
   /** A Map<String,Statistic> of the statistics held by a given
    * Stats implementation
    */
   private Map statistics;

   // Constructors --------------------------------------------------

   public StatsBase()
   {
      statistics = new HashMap();
   }

   public StatsBase(Map statistics)
   {
      this.statistics = statistics;
   }


// Begin Stats interface methods

   /** Access all the Statistics names
    *
    * @return An array of the names of the statistics held in the Stats object
    */
   public String[] getStatisticNames()
   {
      String[] names = new String[statistics.size()];
      statistics.keySet().toArray(names);
      return names;
   }

   /** Access all the Statistics
    *
    * @return An array of the Statistic held in the Stats object
    */
   public Statistic[] getStatistics()
   {
      Statistic[] stats = new Statistic[statistics.size()];
      statistics.values().toArray(stats);
      return stats;
   }

   /** Access a Statistic by its name.
    *
    * @param name
    * @return
    */
   public Statistic getStatistic(String name)
   {
      Statistic stat = (Statistic) statistics.get(name);
      return stat;
   }
// End Stats interface methods

   /** Reset all StatisticImpl objects
    */
   public void reset()
   {
      Iterator iter = statistics.values().iterator();
      while( iter.hasNext() )
      {
         Object next = iter.next();
         if( next instanceof StatisticImpl )
         {
            StatisticImpl s = (StatisticImpl) next;
            s.reset();
         }
      }
   }

   public String toString()
   {
      return this.getClass().getName() + " [ " + statistics + " ]";
   }

   /** Add or replace Statistic in the Stats collection.
    *
    * @param name Name of the Statistic instance
    * @param statistic Statistic to be added
    */
   public void addStatistic(String name, Statistic statistic)
   {
      statistics.put(name, statistic);
   }

}
