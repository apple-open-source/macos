/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This abstract syntax node represents an is empty condition.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class ASTIsEmpty extends SimpleNode {
   public boolean not;

   public ASTIsEmpty(int id) {
      super(id);
   }

   public ASTIsEmpty(EJBQLParser p, int id) {
      super(p, id);
   }

   public String toString() {
      return "IS " + (not ? "NOT " : "") + "EMPTY";
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
