/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This abstract syntax node represents a comparison.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public abstract class ASTComparison extends SimpleNode {
   public String opp;

   public ASTComparison(int id) {
      super(id);
   }

   public ASTComparison(EJBQLParser p, int id) {
      super(p, id);
   }

   public String toString() {
      return EJBQLParserTreeConstants.jjtNodeName[id] + " " + opp;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
