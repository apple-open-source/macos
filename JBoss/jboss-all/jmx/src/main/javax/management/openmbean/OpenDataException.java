/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import javax.management.JMException;

/**
 * Thrown when an open type, open data or open mbean meta data object is
 * attempted to be constructed that is not valid.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class OpenDataException
   extends JMException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final long serialVersionUID = 8346311255433349870L;

   // Constructors --------------------------------------------------

   /**
    * Construct an open data exception with no message.
    */
   public OpenDataException()
   {
      super();
   }

   /**
    * Construct an open data exception with the passed message.
    *
    * @param message the message
    */
   public OpenDataException(String message)
   {
      super(message);
   }
}

