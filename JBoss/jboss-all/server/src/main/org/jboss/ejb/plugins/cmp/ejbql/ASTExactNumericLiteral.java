/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This abstract syntax node represents an exact numeric literal.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class ASTExactNumericLiteral extends SimpleNode {
   public long value;
   public String literal;

   public ASTExactNumericLiteral(int id) {
      super(id);
   }

   public ASTExactNumericLiteral(EJBQLParser p, int id) {
      super(p, id);
   }

   public void setValue(String number) {
      literal = number;

      // long suffix
      if(number.endsWith("l") || number.endsWith("L")) {
         // chop off the suffix
         number = number.substring(0, number.length() - 1);
      }
      
      // hex
      if(number.startsWith("0X") || number.startsWith("0x")) {
         // handle literals from 0x8000000000000000L to 0xffffffffffffffffL:
         // remove sign bit, parse as positive, then calculate the negative 
         // value with the sign bit
         if(number.length() == 18) {
            byte first = Byte.decode(number.substring(0, 3)).byteValue();
            if(first >= 8) {
               number = "0x" + (first - 8) + number.substring(3);
               value = Long.decode(number).longValue() - Long.MAX_VALUE - 1;
               return;
            }
         }
      } else if(number.startsWith("0")) {   // octal
         // handle literals 
         // from 01000000000000000000000L to 01777777777777777777777L
         // remove sign bit, parse as positive, then calculate the 
         // negative value with the sign bit
         if(number.length() == 23) {
            if(number.charAt(1) == '1') {
               number = "0" + number.substring(2);
               value = Long.decode(number).longValue() - Long.MAX_VALUE - 1;
               return;
            }
         }
      }
      value = Long.decode(number).longValue();
   }

   public String toString() {
      return literal;
   }
   
   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
