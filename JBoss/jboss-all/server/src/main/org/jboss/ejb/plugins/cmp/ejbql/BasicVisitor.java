/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This a basic abstract syntax tree visitor.  It simply converts the tree
 * back into ejbql.  This is useful for testing and extensions, as most 
 * extensions translate just a few elements of the tree.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.4.2.1 $
 */                            
public class BasicVisitor implements JBossQLParserVisitor {
   public Object visit(SimpleNode node, Object data) {
      return data;
   }

   public Object visit(ASTEJBQL node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      for(int i=0; i < node.jjtGetNumChildren(); i++) {
         if(i > 0) {
            buf.append(" ");
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTFrom node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("FROM ");
      for(int i=0; i < node.jjtGetNumChildren(); i++) {
         if(i > 0) {
            buf.append(", ");
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTCollectionMemberDeclaration node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("IN(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(") ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTRangeVariableDeclaration node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTSelect node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("SELECT ");
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTWhere node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("WHERE ");
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTOr node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      for(int i=0; i < node.jjtGetNumChildren(); i++) {
         if(i > 0) {
            buf.append(" OR ");
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTAnd node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      for(int i=0; i < node.jjtGetNumChildren(); i++) {
         if(i > 0) {
            buf.append(" AND ");
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTNot node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("NOT ");
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTConditionalParenthetical node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTBetween node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not) {
         buf.append(" NOT");
      }
      buf.append(" BETWEEN ");
      node.jjtGetChild(1).jjtAccept(this, data);
      buf.append(" AND ");
      node.jjtGetChild(2).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTIn node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not) {
         buf.append(" NOT");
      }
      buf.append(" IN(");
      node.jjtGetChild(1).jjtAccept(this, data);
      for(int i=2; i < node.jjtGetNumChildren(); i++) {
         buf.append(", ");
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      buf.append(")");
      return data;
   }

   public Object visit(ASTLike node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not) {
         buf.append(" NOT");
      }
      buf.append(" LIKE ");
      node.jjtGetChild(1).jjtAccept(this, data);
      if(node.jjtGetNumChildren()==3) {
         buf.append(" ESCAPE ");
         node.jjtGetChild(2).jjtAccept(this, data);
      }
      return data;
   }

   
   public Object visit(ASTNullComparison node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" IS ");
      if(node.not) {
         buf.append("NOT");
      }
      buf.append(" NULL");
      return data;
   }

   public Object visit(ASTIsEmpty node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" IS ");
      if(node.not) {
         buf.append("NOT");
      }
      buf.append(" EMPTY");
      return data;
   }

   public Object visit(ASTMemberOf node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not) {
         buf.append("NOT");
      }
      buf.append(" MEMBER OF ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTStringComparison node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" ").append(node.opp).append(" ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTBooleanComparison node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.jjtGetNumChildren()==2) {
         buf.append(" ").append(node.opp).append(" ");
         node.jjtGetChild(1).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTDatetimeComparison node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" ").append(node.opp).append(" ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTEntityComparison node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" ").append(node.opp).append(" ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTValueClassComparison node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" ").append(node.opp).append(" ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTArithmeticComparison node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(" ").append(node.opp).append(" ");
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTPlusMinus node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      for(int i=0; i < node.jjtGetNumChildren(); i++) {
         if(i > 0) {
            buf.append(" ").append(node.opps.get(i-1)).append(" ");
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTMultDiv node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      for(int i=0; i < node.jjtGetNumChildren(); i++) {
         if(i > 0) {
            buf.append(" ").append(node.opps.get(i-1)).append(" ");
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTNegation node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("-");
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTArithmeticParenthetical node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTStringParenthetical node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTConcat node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("CONCAT(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(", ");
      node.jjtGetChild(1).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTSubstring node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("SUBSTRING(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(", ");
      node.jjtGetChild(1).jjtAccept(this, data);
      buf.append(", ");
      node.jjtGetChild(2).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTLCase node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("LCASE(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTUCase node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("UCASE(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTLength node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("LENGTH(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTLocate node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("LOCATE(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(", ");
      node.jjtGetChild(1).jjtAccept(this, data);
      if(node.jjtGetNumChildren()==3) {
         buf.append(", ");
         node.jjtGetChild(2).jjtAccept(this, data);
      }
      buf.append(")");
      return data;
   }

   public Object visit(ASTAbs node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("ABS(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTSqrt node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("SQRT(");
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(")");
      return data;
   }

   public Object visit(ASTOrderBy node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;

      buf.append("ORDER BY ");
      for(int i=0; i < node.jjtGetNumChildren(); i++) {
         if(i > 0) {
            buf.append(", ");
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTOrderByPath node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;

      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.ascending) {
         buf.append(" ASC");
      } else {
         buf.append(" DESC");
      }
      return data;
   }

   public Object visit(ASTPath node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append(node.getPath());
      return data;
   }

   public Object visit(ASTIdentifier node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append(node.identifier);
      return data;
   }

   public Object visit(ASTAbstractSchema node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append(node.abstractSchemaName);
      return data;
   }

   public Object visit(ASTParameter node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append("?").append(node.number);
      return data;
   }

   public Object visit(ASTExactNumericLiteral node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append(node.literal);
      return data;
   }

   public Object visit(ASTApproximateNumericLiteral node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append(node.literal);
      return data;
   }

   public Object visit(ASTStringLiteral node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append(node.value);
      return data;
   }

   public Object visit(ASTBooleanLiteral node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      buf.append(node.value);
      return data;
   }

   public Object visit(ASTLimitOffset node, Object data) {
      BlockStringBuffer buf = (BlockStringBuffer)data;
      int child = 0;
      if (node.hasOffset) {
         buf.append(" OFFSET ");
         node.jjtGetChild(child++).jjtAccept(this, data);
      }
      if (node.hasLimit) {
         buf.append(" LIMIT ");
         node.jjtGetChild(child++).jjtAccept(this, data);
      }
      return data;
   }
}
