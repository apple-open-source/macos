/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.Stats;
import javax.management.j2ee.statistics.CountStatistic;

/**
 * Specifies statistics provided by all EJB component types as defined by
 * JSR77.6.11.
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface EJBStats
   extends Stats
{
   /**
    * @return Returns the number of times the beans create method was called
    */
   public CountStatistic getCreateCount();
   
   /**
    * @return Returns the number of times the beans remove method was called
    */
   public CountStatistic getRemoveCount();
}
