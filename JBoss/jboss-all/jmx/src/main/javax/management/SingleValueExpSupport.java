/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * An implementation of single value expression.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1 $
 */
/*package*/ class SingleValueExpSupport
   extends ValueExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The value of this object
    */
   private Object value;

   // Static ------------------------------------------------------

   // Constructor -------------------------------------------------

   /**
    * Construct a value expression for the passed value
    *
    * @param value the value
    */
   public SingleValueExpSupport(Object value)
   {
      this.value = value;
   }

   // Public ------------------------------------------------------

   /**
    * Get the value of the expression
    */
   public Object getValue()
   {
      return value;
   }

   // X implementation --------------------------------------------

   // Object overrides --------------------------------------------

   public String toString()
   {
      return value.toString();
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
