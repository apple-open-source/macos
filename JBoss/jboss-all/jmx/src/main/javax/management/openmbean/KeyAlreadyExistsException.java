/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

/**
 * Thrown when a row index for tabular data already exists.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class KeyAlreadyExistsException
   extends IllegalArgumentException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final long serialVersionUID = 1845183636745282866L;

   // Constructors --------------------------------------------------

   /**
    * Construct an key already exsits exception with no message.
    */
   public KeyAlreadyExistsException()
   {
      super();
   }

   /**
    * Construct an key already exists exception with the passed message.
    *
    * @param message the message
    */
   public KeyAlreadyExistsException(String message)
   {
      super(message);
   }
}

