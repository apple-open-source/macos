/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;


/**
 * Thrown when an invalid string operation is passed to a query construction
 * method.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class BadStringOperationException
   extends Exception
{
   // Attributes ----------------------------------------------------

   /**
    * The operation
    */
   private String op;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new BadStringOperationException with the given operation.
    *
    * @param op the invalid operation.
    */
   public BadStringOperationException(String op)
   {
      super(op);
      this.op = op;
   }

   // Public --------------------------------------------------------

   // Exception Overrides -------------------------------------------

   /**
    * Returns a string representing the error.
    *
    * @return the error string.
    */
   public String toString()
   {
      return "Bad string operation: " + op;
   }
}
