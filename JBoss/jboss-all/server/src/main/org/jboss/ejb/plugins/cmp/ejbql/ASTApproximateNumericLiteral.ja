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
 * @version $Revision: 1.1 $
 */                            
public class ASTApproximateNumericLiteral extends SimpleNode {
   public double value;
   public String literal;

   public ASTApproximateNumericLiteral(int id) {
      super(id);
   }

   public ASTApproximateNumericLiteral(EJBQLParser p, int id) {
      super(p, id);
   }

   public void setValue(String number) {
      literal = number;

      // float suffix
      if(number.endsWith("f") || number.endsWith("F")) {
         // chop off the suffix
         number = number.substring(0, number.length()-1);
         value = Float.parseFloat(number);
      } else {
         // ends with a d suffix, chop it off
         if(number.endsWith("d") || number.endsWith("D")) {
            number = number.substring(0, number.length()-1);
         }

         // regular double
         value = Double.parseDouble(number);
      }
   }

   public String toString() {
      return literal;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
