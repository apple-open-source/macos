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
 * @version $Revision: 1.1.4.2 $
 */                            
public final class ASTExactNumericLiteral extends SimpleNode {
   public long value;
   public String literal;
   private static final String LOFFER_L = "l";
   private static final String UPPER_L = "L";
   private static final String OX = "0X";
   private static final String Ox = "0x";
   private static final String ZERRO = "0";

   public ASTExactNumericLiteral(int id) {
      super(id);
   }

   public void setValue(String number) {
      literal = number;

      // long suffix
      if(number.endsWith(LOFFER_L) || number.endsWith(UPPER_L)) {
         // chop off the suffix
         number = number.substring(0, number.length() - 1);
      }
      
      // hex
      if(number.startsWith(OX) || number.startsWith(Ox)) {
         // handle literals from 0x8000000000000000L to 0xffffffffffffffffL:
         // remove sign bit, parse as positive, then calculate the negative 
         // value with the sign bit
         if(number.length() == 18) {
            byte first = Byte.decode(number.substring(0, 3)).byteValue();
            if(first >= 8) {
               number = Ox + (first - 8) + number.substring(3);
               value = Long.decode(number).longValue() - Long.MAX_VALUE - 1;
               return;
            }
         }
      } else if(number.startsWith(ZERRO)) {   // octal
         // handle literals 
         // from 01000000000000000000000L to 01777777777777777777777L
         // remove sign bit, parse as positive, then calculate the 
         // negative value with the sign bit
         if(number.length() == 23) {
            if(number.charAt(1) == '1') {
               number = ZERRO + number.substring(2);
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
