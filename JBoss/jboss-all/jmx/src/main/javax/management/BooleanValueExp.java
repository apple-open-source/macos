/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A Boolean that is an arguement to a query.<p>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1 $
 */
/*package*/ class BooleanValueExp
   extends SingleValueExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   // Static  -----------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a boolean value expression for the passed boolean
    *
    * @param value the boolean value
    */
   public BooleanValueExp(Boolean value)
   {
      super(value);
   }

   // Public ------------------------------------------------------

   // X Implementation --------------------------------------------

   // Y overrides -------------------------------------------------

   // Protected ---------------------------------------------------

   // Package Private ---------------------------------------------

   // Private -----------------------------------------------------

   // Inner Classes -----------------------------------------------
}
