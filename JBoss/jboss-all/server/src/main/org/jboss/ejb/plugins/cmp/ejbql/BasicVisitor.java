/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.ejbql;

import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;

/**
 * This a basic abstract syntax tree visitor.  It simply converts the tree
 * back into ejbql.  This is useful for testing and extensions, as most
 * extensions translate just a few elements of the tree.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.4.2.7 $
 */
public class BasicVisitor implements JBossQLParserVisitor
{
   public Object visit(SimpleNode node, Object data)
   {
      return data;
   }

   public Object visit(ASTEJBQL node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      for(int i = 0; i < node.jjtGetNumChildren(); i++)
      {
         if(i > 0)
         {
            buf.append(' ');
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTFrom node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.FROM);
      for(int i = 0; i < node.jjtGetNumChildren(); i++)
      {
         if(i > 0)
         {
            buf.append(SQLUtil.COMMA);
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTCollectionMemberDeclaration node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.IN).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')').append(' ');
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTRangeVariableDeclaration node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(' ');
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTSelect node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.SELECT);
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTWhere node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.WHERE);
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTOr node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      for(int i = 1; i < node.jjtGetNumChildren(); ++i)
      {
         buf.append(SQLUtil.OR);
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTWhereConditionalTerm node, Object data)
   {
      for(int i = 0; i < node.jjtGetNumChildren(); ++i)
      {
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTAnd node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      for(int i = 1; i < node.jjtGetNumChildren(); i++)
      {
         buf.append(SQLUtil.AND);
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTNot node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.NOT);
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTConditionalParenthetical node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTBetween node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not)
      {
         buf.append(SQLUtil.NOT);
      }
      buf.append(SQLUtil.BETWEEN);
      node.jjtGetChild(1).jjtAccept(this, data);
      buf.append(SQLUtil.AND);
      node.jjtGetChild(2).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTIn node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not)
      {
         buf.append(SQLUtil.NOT);
      }
      buf.append(SQLUtil.IN).append('(');
      node.jjtGetChild(1).jjtAccept(this, data);
      for(int i = 2; i < node.jjtGetNumChildren(); i++)
      {
         buf.append(SQLUtil.COMMA);
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      buf.append(')');
      return data;
   }

   public Object visit(ASTLike node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not)
      {
         buf.append(SQLUtil.NOT);
      }
      buf.append(SQLUtil.LIKE);
      node.jjtGetChild(1).jjtAccept(this, data);
      if(node.jjtGetNumChildren() == 3)
      {
         buf.append(SQLUtil.ESCAPE);
         node.jjtGetChild(2).jjtAccept(this, data);
      }
      return data;
   }


   public Object visit(ASTNullComparison node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(SQLUtil.IS);
      if(node.not)
      {
         buf.append(SQLUtil.NOT);
      }
      buf.append(SQLUtil.NULL);
      return data;
   }

   public Object visit(ASTIsEmpty node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(SQLUtil.IS);
      if(node.not)
      {
         buf.append(SQLUtil.NOT);
      }
      buf.append(SQLUtil.EMPTY);
      return data;
   }

   public Object visit(ASTMemberOf node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.not)
      {
         buf.append(SQLUtil.NOT);
      }
      buf.append(SQLUtil.MEMBER_OF);
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTStringComparison node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(' ').append(node.opp).append(' ');
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTBooleanComparison node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.jjtGetNumChildren() == 2)
      {
         buf.append(' ').append(node.opp).append(' ');
         node.jjtGetChild(1).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTDatetimeComparison node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(' ').append(node.opp).append(' ');
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTEntityComparison node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(' ').append(node.opp).append(' ');
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTValueClassComparison node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(' ').append(node.opp).append(' ');
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTArithmeticComparison node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(' ').append(node.opp).append(' ');
      node.jjtGetChild(1).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTPlusMinus node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      for(int i = 0; i < node.jjtGetNumChildren(); i++)
      {
         if(i > 0)
         {
            buf.append(' ').append(node.opps.get(i - 1)).append(' ');
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTMultDiv node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      for(int i = 0; i < node.jjtGetNumChildren(); i++)
      {
         if(i > 0)
         {
            buf.append(' ').append(node.opps.get(i - 1)).append(' ');
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTNegation node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append('-');
      node.jjtGetChild(0).jjtAccept(this, data);
      return data;
   }

   public Object visit(ASTArithmeticParenthetical node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTStringParenthetical node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTConcat node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.CONCAT).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(SQLUtil.COMMA);
      node.jjtGetChild(1).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTSubstring node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.SUBSTRING).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(SQLUtil.COMMA);
      node.jjtGetChild(1).jjtAccept(this, data);
      buf.append(SQLUtil.COMMA);
      node.jjtGetChild(2).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTLCase node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.LCASE).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTUCase node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.UCASE).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTLength node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.LENGTH).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTLocate node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.LOCATE).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(SQLUtil.COMMA);
      node.jjtGetChild(1).jjtAccept(this, data);
      if(node.jjtGetNumChildren() == 3)
      {
         buf.append(SQLUtil.COMMA);
         node.jjtGetChild(2).jjtAccept(this, data);
      }
      buf.append(')');
      return data;
   }

   public Object visit(ASTAbs node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.ABS).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTSqrt node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.SQRT).append('(');
      node.jjtGetChild(0).jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTCount node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.COUNT).append('(');
      ASTPath path = (ASTPath)node.jjtGetChild(0);
      path.children[0].jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTMax node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.MAX).append('(');
      ASTPath path = (ASTPath)node.jjtGetChild(0);
      path.children[0].jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTMin node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.MIN).append('(');
      ASTPath path = (ASTPath)node.jjtGetChild(0);
      path.children[0].jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTAvg node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.AVG).append('(');
      ASTPath path = (ASTPath)node.jjtGetChild(0);
      path.children[0].jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTSum node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(SQLUtil.SUM).append('(');
      ASTPath path = (ASTPath)node.jjtGetChild(0);
      path.children[0].jjtAccept(this, data);
      buf.append(')');
      return data;
   }

   public Object visit(ASTOrderBy node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;

      buf.append(SQLUtil.ORDERBY);
      for(int i = 0; i < node.jjtGetNumChildren(); i++)
      {
         if(i > 0)
         {
            buf.append(SQLUtil.COMMA);
         }
         node.jjtGetChild(i).jjtAccept(this, data);
      }
      return data;
   }

   public Object visit(ASTOrderByPath node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;

      node.jjtGetChild(0).jjtAccept(this, data);
      if(node.ascending)
      {
         buf.append(SQLUtil.ASC);
      }
      else
      {
         buf.append(SQLUtil.DESC);
      }
      return data;
   }

   public Object visit(ASTPath node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(node.getPath());
      return data;
   }

   public Object visit(ASTIdentifier node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(node.identifier);
      return data;
   }

   public Object visit(ASTAbstractSchema node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(node.abstractSchemaName);
      return data;
   }

   public Object visit(ASTParameter node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append("?").append(node.number);
      return data;
   }

   public Object visit(ASTExactNumericLiteral node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(node.literal);
      return data;
   }

   public Object visit(ASTApproximateNumericLiteral node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(node.literal);
      return data;
   }

   public Object visit(ASTStringLiteral node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(node.value);
      return data;
   }

   public Object visit(ASTBooleanLiteral node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      buf.append(node.value);
      return data;
   }

   public Object visit(ASTLimitOffset node, Object data)
   {
      StringBuffer buf = (StringBuffer)data;
      int child = 0;
      if(node.hasOffset)
      {
         buf.append(SQLUtil.OFFSET);
         node.jjtGetChild(child++).jjtAccept(this, data);
      }
      if(node.hasLimit)
      {
         buf.append(SQLUtil.LIMIT);
         node.jjtGetChild(child).jjtAccept(this, data);
      }
      return data;
   }
}
