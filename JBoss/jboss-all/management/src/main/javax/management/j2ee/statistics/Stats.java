/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

import java.io.Serializable;

/**
 * Represents specific performance data attributes for each
 * of the specific managed object types.
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface Stats
   extends Serializable
{
   /**
    * @return The list of names of attributes for the specific Stats submodel
    *         that this object supports. Attributes named in the list match
    *         the attributes that will return a Statistic object of the
    *         appropriate type.
    **/
   public String[] getStatisticNames();

   /**
    * @return The list of Statistics objects supported by this Stats object
    **/
   public Statistic[] getStatistics();

   /**
    * Delivers a Statistic by its given name
    *
    * @param pName Name of the Statistic to look up.
    *
    * @return A Statistic if the given name is found otherwise null
    **/
   public Statistic getStatistic( String pName );
}
