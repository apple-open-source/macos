/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

/**
 * Thrown when an item name for composite data or a row index for tabular
 * data is invalid.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class InvalidKeyException
   extends IllegalArgumentException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final long serialVersionUID = 4224269443946322062L;

   // Constructors --------------------------------------------------

   /**
    * Construct an invalid key exception with no message.
    */
   public InvalidKeyException()
   {
      super();
   }

   /**
    * Construct an invalid key exception with the passed message.
    *
    * @param message the message
    */
   public InvalidKeyException(String message)
   {
      super(message);
   }
}

