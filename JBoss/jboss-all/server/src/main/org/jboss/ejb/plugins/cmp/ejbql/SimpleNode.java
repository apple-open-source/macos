/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * Parent class of all abstract syntax tree nodes.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class SimpleNode implements Node {
   protected Node parent;
   protected Node[] children;
   protected int id;
   protected EJBQLParser parser;

   public SimpleNode(int i) {
      id = i;
   }

   public SimpleNode(EJBQLParser p, int i) {
      this(i);
      parser = p;
   }

   public void jjtOpen() {
   }

   public void jjtClose() {
   }
   
   public void jjtSetParent(Node n) { parent = n; }
   public Node jjtGetParent() { return parent; }

   public void jjtAddChild(Node n, int i) {
      if (children == null) {
         children = new Node[i + 1];
      } else if (i >= children.length) {
         Node c[] = new Node[i + 1];
         System.arraycopy(children, 0, c, 0, children.length);
         children = c;
      }
      children[i] = n;
   }

   public Node jjtGetChild(int i) {
      return children[i];
   }

   public int jjtGetNumChildren() {
      return (children == null) ? 0 : children.length;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }

   /** Accept the visitor. **/
   public Object childrenAccept(JBossQLParserVisitor visitor, Object data) {
      if (children != null) {
         for (int i = 0; i < children.length; ++i) {
            children[i].jjtAccept(visitor, data);
         }
      }
      return data;
   }

   /* You can override these two methods in subclasses of SimpleNode to
       customize the way the node appears when the tree is dumped.   If
       your output uses more than one line you should override
       toString(String), otherwise overriding toString() is probably all
       you need to do. */

   public String toString() { return EJBQLParserTreeConstants.jjtNodeName[id]; }
   public String toString(String prefix) { return prefix + toString(); }

   /* Override this method if you want to customize how the node dumps
       out its children. */

   public void dump(String prefix) {
      System.out.println(toString(prefix));
      if (children != null) {
         for (int i = 0; i < children.length; ++i) {
            SimpleNode n = (SimpleNode)children[i];
            if (n != null) {
               n.dump(prefix + " ");
            }
         }
      }
   }
}

