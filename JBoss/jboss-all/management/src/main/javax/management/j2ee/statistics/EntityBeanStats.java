/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

/**
 * Specifies statistics provided by entity beans as defined by JSR77.6.12.
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface EntityBeanStats
      extends EJBStats
{
   /**
    * @return Returns the number of bean instances in the ready state
    */
   public RangeStatistic getReadyCount();

   /**
    * @return Returns the number of bean instances in the pooled state
    */
   public RangeStatistic getPooledCount();
}
