/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

/** Represents a standard measurements of the upper and lower
 * limits of the value of an attribute as defined by JSR77.6.8
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface BoundaryStatistic
   extends Statistic
{
   /**
    * @return The lower limit of the value of the attribute
    */
   public long getLowerBound();

   /**
    * @return The upper limit of the value of the attribute
    */
   public long getUpperBound();
}
