/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import javax.management.JMException;

/**
 * A super class for all relations thrown during relation management.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class RelationException
   extends JMException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RelationException with no message.
    */
   public RelationException()
   {
      super();
   }

   /**
    * Construct a new RelationException with the given message.
    *
    * @param message the error message.
    */
   public RelationException(String message)
   {
      super(message);
   }
}

