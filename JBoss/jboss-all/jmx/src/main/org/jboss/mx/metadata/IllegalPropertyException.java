/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.metadata;

/**
 * Exception thrown when a property has an illegal value.
 *
 * @see org.jboss.mx.metadata.AbstractBuilder
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class IllegalPropertyException
   extends Exception
{
   // Constructors --------------------------------------------------
   public IllegalPropertyException(String msg)
   {
      super(msg);
   }
   
}
      



