/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A Binary Operation that is an arguement to a query.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020314 Adrian Brock:</b>
 * <ul>
 * <li>Added human readable string representation.            
 * </ul>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.4 $
 */
/*package*/ class BinaryOpValueExp
   extends ValueExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The operation
    */
   int operation;

   /**
    * The first expression
    */
   ValueExp first;

   /**
    * The second expression
    */
   ValueExp second;

   // Static  -----------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a binary operation value
    *
    * @param operation the operation as defined in Query
    * @param first the first expression in the operation
    * @param second the second expression in the operation
    */
   public BinaryOpValueExp(int operation, ValueExp first, ValueExp second)
   {
      this.operation = operation;
      this.first = first;
      this.second = second;
   }

   // Public ------------------------------------------------------

   // Value Exp Implementation ------------------------------------

   public ValueExp apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      ValueExp testFirst = first.apply(name);
      ValueExp testSecond = second.apply(name);

      if (testFirst instanceof NumberValueExp)
      {
         if (((NumberValueExp)testFirst).isInteger())
         {
            switch (operation)
            {
            case Query.PLUS:
               return Query.value(((NumberValueExp)testFirst).getLongValue() + 
                                  ((NumberValueExp)testSecond).getLongValue());
            case Query.MINUS:
               return Query.value(((NumberValueExp)testFirst).getLongValue() -
                                  ((NumberValueExp)testSecond).getLongValue());
            case Query.TIMES:
               return Query.value(((NumberValueExp)testFirst).getLongValue() *
                                  ((NumberValueExp)testSecond).getLongValue());
            case Query.DIV:
               return Query.value(((NumberValueExp)testFirst).getLongValue() /
                               ((NumberValueExp)testSecond).getLongValue());
            }
         }
         else
         {
            switch (operation)
            {
            case Query.PLUS:
               return Query.value(((NumberValueExp)testFirst).getDoubleValue() + 
                                  ((NumberValueExp)testSecond).getDoubleValue());
            case Query.MINUS:
               return Query.value(((NumberValueExp)testFirst).getDoubleValue() -
                                  ((NumberValueExp)testSecond).getDoubleValue());
            case Query.TIMES:
               return Query.value(((NumberValueExp)testFirst).getDoubleValue() *
                                  ((NumberValueExp)testSecond).getDoubleValue());
            case Query.DIV:
               return Query.value(((NumberValueExp)testFirst).getDoubleValue() /
                               ((NumberValueExp)testSecond).getDoubleValue());
            }
         }
      }
      else if (testFirst instanceof StringValueExp)
      {
         switch (operation)
         {
         case Query.PLUS:
            return Query.value(
               new String(((StringValueExp)testFirst).toString() + 
                          ((StringValueExp)testSecond).toString()));
         }
         throw new BadStringOperationException("TODO");
      }
      // Review What happens now?
      throw new BadBinaryOpValueExpException(testFirst);
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      StringBuffer buffer = new StringBuffer();
      buffer.append("(");
      buffer.append(first);
      buffer.append(")");
      switch (operation)
      {
      case Query.PLUS:
         buffer.append(" + "); break;
      case Query.MINUS:
         buffer.append(" - "); break;
      case Query.TIMES:
         buffer.append(" * "); break;
      case Query.DIV:
         buffer.append(" / ");
      }
      buffer.append("(");
      buffer.append(second);
      buffer.append(")");
      return buffer.toString();
   }

   // Protected ---------------------------------------------------

   // Package Private ---------------------------------------------

   // Private -----------------------------------------------------

   // Inner Classes -----------------------------------------------
}
