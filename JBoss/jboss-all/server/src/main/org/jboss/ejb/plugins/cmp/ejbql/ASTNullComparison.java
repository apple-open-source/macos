/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;

/**
 * This abstract syntax node represents an is null comparison.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.2 $
 */                            
public final class ASTNullComparison extends SimpleNode {
   public boolean not;

   public ASTNullComparison(int id) {
      super(id);
   }

   public String toString() {
      return (not ? SQLUtil.NOT : SQLUtil.EMPTY_STRING) + SQLUtil.NULL;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
