/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when an attempt is made to apply either of the following.
 * A subquery expression to an MBean or a qualified expression to an
 * MBean of the wrong class.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class InvalidApplicationException
   extends Exception
{
   // Attributes ----------------------------------------------------

   private Object val = null;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new IntrospectionException with the specified object.
    *
    * @param val the specified object.
    */
   public InvalidApplicationException(Object val)
   {
      super();
      this.val = val;
   }

   // Exception Overrides -------------------------------------------

   /**
    * Get a string represention of the exception.
    *
    * @return the string representation of the exception.
    */
   public String toString()
   {
      return "Invalid Application: " + val.toString();
   }
}

