/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

/**
 * Represents the statistics provided by session beans
 * of both stateful and stateless types as defined by JSR77.6.14
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface SessionBeanStats
      extends EJBStats
{
   /**
    * @return The number of beans in the method-ready state
    */
   public RangeStatistic getMethodReadyCount();
}
