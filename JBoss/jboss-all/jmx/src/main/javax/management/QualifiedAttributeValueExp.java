/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A qualified string that is an argument to a query.<p>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
/*package*/ class QualifiedAttributeValueExp
   extends AttributeValueExp
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The class name
    */
   private String className;

   // Static  -----------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct an attribute value expression for the passed class and 
    * attribute name
    *
    * @param className the class name
    * @param value the attribute name
    */
   public QualifiedAttributeValueExp(String className, String value)
   {
      super(value);
      this.className = className;
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
         if (instance.getClassName().equals(className))
            return super.apply(name);
      }
      catch (Exception e)
      {
         // REVIEW: What happens here? Should this happen?
         return null;
      }
      throw new InvalidApplicationException(new String(name + "\n" + className));
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String(className + "." + getAttributeName());
   }

   // Protected ---------------------------------------------------

   // Package Private ---------------------------------------------

   // Private -----------------------------------------------------

   // Inner Classes -----------------------------------------------
}
