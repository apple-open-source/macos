/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This abstract syntax node represents a parameter declaration.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class ASTParameter extends SimpleNode {
   public int number;
   public int type;

   public ASTParameter(int id) {
      super(id);
   }

   public ASTParameter(EJBQLParser p, int id) {
      super(p, id);
   }

   public String toString() {
      return "?" + number + " <" + type + ">";
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
