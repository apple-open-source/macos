/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;

import java.sql.ResultSet;
import java.sql.SQLException;


/**
 * This abstract syntax node represents MIN function.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @version $Revision: 1.1.2.2 $
 */                            
public final class ASTMin
   extends SimpleNode
   implements SelectFunction
{
   public ASTMin(int id) {
      super(id);
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }

   // SelectFunction implementation

   public Object readResult(ResultSet rs) throws SQLException
   {
      return JDBCUtil.DOUBLE_READER.getFirst(rs, Double.class);
   }
}
