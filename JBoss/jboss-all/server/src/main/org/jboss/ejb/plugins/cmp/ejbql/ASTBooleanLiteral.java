/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This abstract syntax node represents a boolean literal.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class ASTBooleanLiteral extends SimpleNode {
   public boolean value;

   public ASTBooleanLiteral(int id) {
      super(id);
   }

   public ASTBooleanLiteral(EJBQLParser p, int id) {
      super(p, id);
   }

   public String toString() {
      return "" + value;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
