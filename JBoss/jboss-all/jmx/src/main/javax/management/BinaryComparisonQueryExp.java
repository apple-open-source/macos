/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A Binary Comparison query.
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
 * @version $Revision: 1.3 $
 */
/*package*/ class BinaryComparisonQueryExp
   extends QueryExpSupport
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
    * Construct a binary comparison query
    *
    * @param operation the comparison as defined in Query
    * @param first the first expression in the query
    * @param second the second expression in the query
    */
   public BinaryComparisonQueryExp(int operation, ValueExp first, ValueExp second)
   {
      this.operation = operation;
      this.first = first;
      this.second = second;
   }

   // Public ------------------------------------------------------

   // Query Exp Implementation ------------------------------------

   public boolean apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      ValueExp testFirst = first.apply(name);
      ValueExp testSecond = second.apply(name);

      if (testFirst instanceof NumberValueExp)
      {
         switch (operation)
         {
         case Query.GT:
            return ((NumberValueExp)testFirst).getDoubleValue() > 
                   ((NumberValueExp)testSecond).getDoubleValue();
         case Query.GE:
            return ((NumberValueExp)testFirst).getDoubleValue() >=
                   ((NumberValueExp)testSecond).getDoubleValue();
         case Query.LT:
            return ((NumberValueExp)testFirst).getDoubleValue() <
                   ((NumberValueExp)testSecond).getDoubleValue();
         case Query.LE:
            return ((NumberValueExp)testFirst).getDoubleValue() <=
                   ((NumberValueExp)testSecond).getDoubleValue();
         case Query.EQ:
            return ((NumberValueExp)testFirst).getDoubleValue() ==
                   ((NumberValueExp)testSecond).getDoubleValue();
         }
      }
      else if (testFirst instanceof StringValueExp)
      {
         switch (operation)
         {
         case Query.GT:
            return ((StringValueExp)testFirst).toString().compareTo( 
                   ((StringValueExp)testSecond).toString()) > 0;
         case Query.GE:
            return ((StringValueExp)testFirst).toString().compareTo( 
                   ((StringValueExp)testSecond).toString()) >= 0;
         case Query.LT:
            return ((StringValueExp)testFirst).toString().compareTo( 
                   ((StringValueExp)testSecond).toString()) < 0;
         case Query.LE:
            return ((StringValueExp)testFirst).toString().compareTo( 
                   ((StringValueExp)testSecond).toString()) <= 0;
         case Query.EQ:
            return ((StringValueExp)testFirst).toString().compareTo( 
                   ((StringValueExp)testSecond).toString()) == 0;
         }
         throw new BadStringOperationException("TODO");
      }
      else if (testFirst instanceof SingleValueExpSupport)
      {
         switch (operation)
         {
         case Query.EQ:
            return ((SingleValueExpSupport)testFirst).getValue().equals( 
                   ((SingleValueExpSupport)testSecond).getValue());
         }
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
      case Query.GT:
         buffer.append(" > "); break;
      case Query.GE:
         buffer.append(" >= "); break;
      case Query.LT:
         buffer.append(" < "); break;
      case Query.LE:
         buffer.append(" <= "); break;
      case Query.EQ:
         buffer.append(" == ");
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
