/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

/**
 * Thrown when an open type of an open data is not correct.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class InvalidOpenTypeException
   extends IllegalArgumentException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final long serialVersionUID = -2837312755412327534L;

   // Constructors --------------------------------------------------

   /**
    * Construct an invalid open type exception with no message.
    */
   public InvalidOpenTypeException()
   {
      super();
   }

   /**
    * Construct an invalid opent type with the passed message.
    *
    * @param message the message
    */
   public InvalidOpenTypeException(String message)
   {
      super(message);
   }
}

