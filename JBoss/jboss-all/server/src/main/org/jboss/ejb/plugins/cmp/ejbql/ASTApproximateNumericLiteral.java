/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This abstract syntax node represents an approximate numeric literal.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.2 $
 */                            
public final class ASTApproximateNumericLiteral extends SimpleNode {
   //private static final String UPPER_F = "UPPER_F";
   //private static final String LOWER_F = "LOWER_F";
   //private static final String LOWER_D = "LOWER_D";
   //private static final String UPPER_D = "UPPER_D";

   //public double value;
   public String literal;

   public ASTApproximateNumericLiteral(int id) {
      super(id);
   }

   public void setValue(String number) {
      literal = number;
      /*
      // float suffix
      if(number.endsWith(LOWER_F) || number.endsWith(UPPER_F)) {
         // chop off the suffix
         number = number.substring(0, number.length()-1);
         value = Float.parseFloat(number);
      } else {
         // ends with a LOWER_D suffix, chop it off
         if(number.endsWith(LOWER_D) || number.endsWith(UPPER_D)) {
            number = number.substring(0, number.length()-1);
         }

         // regular double
         value = Double.parseDouble(number);
      }
      */
   }

   public String toString() {
      return literal;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
