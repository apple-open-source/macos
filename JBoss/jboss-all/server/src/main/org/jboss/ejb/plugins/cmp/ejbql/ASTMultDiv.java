/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import java.util.ArrayList;
import java.util.List;

/**
 * This abstract syntax node represents a series of multiplication and
 * divide opperators.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class ASTMultDiv extends SimpleNode {
   public List opps = new ArrayList();

   public ASTMultDiv(int id) {
      super(id);
   }

   public ASTMultDiv(EJBQLParser p, int id) {
      super(p, id);
   }

   public void addOpp(String opp) {
      opps.add(opp);
   }

   public String toString() {
      return "MultDiv " + opps;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
