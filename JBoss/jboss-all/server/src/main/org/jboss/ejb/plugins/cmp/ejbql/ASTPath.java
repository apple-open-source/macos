/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import java.util.List;
import org.jboss.ejb.plugins.cmp.bridge.EntityBridge;
import org.jboss.ejb.plugins.cmp.bridge.FieldBridge;
import org.jboss.ejb.plugins.cmp.bridge.CMPFieldBridge;
import org.jboss.ejb.plugins.cmp.bridge.CMRFieldBridge;

/**
 * This abstract syntax node represents a path declaration.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.1 $
 */                            
public final class ASTPath extends SimpleNode {
   public List pathList;
   public List fieldList;
   public int type;

   public ASTPath(int id) {
      super(id);
   }

   public String getPath() {
      return (String)pathList.get(pathList.size()-1);
   }

   public String getPath(int i) {
      return (String)pathList.get(i);
   }

   public FieldBridge getField() {
      return (FieldBridge)fieldList.get(fieldList.size()-1);
   }

   public boolean isCMPField() {
      return fieldList.get(fieldList.size()-1) instanceof CMPFieldBridge;
   }

   public CMPFieldBridge getCMPField() {
      return (CMPFieldBridge)fieldList.get(fieldList.size()-1);
   }

   public boolean isCMRField() {
      return fieldList.get(fieldList.size()-1) instanceof CMRFieldBridge;
   }

   public boolean isCMRField(int i) {
      return fieldList.get(i) instanceof CMRFieldBridge;
   }

   public CMRFieldBridge getCMRField() {
      return (CMRFieldBridge)fieldList.get(fieldList.size()-1);
   }

   public CMRFieldBridge getCMRField(int i) {
      return (CMRFieldBridge)fieldList.get(i);
   }

   public EntityBridge getEntity() {
      Object field = fieldList.get(fieldList.size()-1);
      if(field instanceof CMRFieldBridge) {
         return ((CMRFieldBridge)field).getRelatedEntity();
      } else if(field instanceof EntityBridge) {
         return (EntityBridge)field;
      } else {
         return null;
      }
   }

   public EntityBridge getEntity(int i) {
      Object field = fieldList.get(i);
      if(field instanceof CMRFieldBridge) {
         return ((CMRFieldBridge)field).getRelatedEntity();
      } else if(field instanceof EntityBridge) {
         return (EntityBridge)field;
      } else {
         return null;
      }
   }

   public int size() {
      return fieldList.size();
   }

   public String toString() {
      return pathList.get(pathList.size()-1) + " <" + type + ">";
   }

   public boolean equals(Object o) {
      if(o instanceof ASTPath) {
         ASTPath path = (ASTPath)o;
         return path.getPath().equals(getPath());
      }
      return false;
   }

   public int hashCode() {
      return getPath().hashCode();
   }

   /** Accept the visitor. **/
   public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
      return visitor.visit(this, data);
   }
}
