/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Get the class name of the mbean
 *
 * <p><b>Revisions:</b>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
/*package*/ class ClassAttributeValueExp
   extends AttributeValueExp
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   // Static  -----------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct an class attribute value expression
    */
   public ClassAttributeValueExp()
   {
      // REVIEW: Correct?
      super(null);
   }

   // Public ------------------------------------------------------

   // ValueExp Implementation -------------------------------------

   public ValueExp apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
       try
       {
          ObjectInstance instance = getMBeanServer().getObjectInstance(name);
          return Query.value(instance.getClassName());
       }
       catch (Exception e)
       {
          // REVIEW: Correct?
          throw new InvalidApplicationException(name);
       }
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String("class");
   }

   // Protected ---------------------------------------------------

   // Package Private ---------------------------------------------

   // Private -----------------------------------------------------

   // Inner Classes -----------------------------------------------
}
