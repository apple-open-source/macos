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
 * This abstract syntax node represents a series of addition and subtraction
 * opperations.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class ASTPlusMinus extends SimpleNode {
   public List opps = new ArrayList();
   
   public ASTPlusMinus(int id) {
      super(id);
   }

   public ASTPlusMinus(EJBQLParser p, int id) {
      super(p, id);
   }

   public void addOpp(String opp) {
      opps.add(opp);
   }

   public String toString() {
      return "PlusMinus " + opps;
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
