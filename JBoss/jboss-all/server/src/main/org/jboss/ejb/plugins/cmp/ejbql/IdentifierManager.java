/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.jboss.ejb.plugins.cmp.bridge.EntityBridge;
import org.jboss.ejb.plugins.cmp.bridge.CMRFieldBridge;

/**
 * This class manages a symbol table for the EJB-QL parser.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.2.4.2 $
 */                            
public final class IdentifierManager {
   private final Catalog catalog;
   private final Map pathLists = new HashMap();
   private final Map fieldLists = new HashMap();
   private final Map identifiers = new HashMap();

   public IdentifierManager(Catalog catalog) {
      this.catalog = catalog;
   }

   public void declareRangeVariable(
         String identifier,
         String abstractSchemaName) {

      identifiers.put(
            identifier, 
            catalog.getEntityByAbstractSchemaName(abstractSchemaName));
   }
     
   public void declareCollectionMember(
         String identifier, 
         String path) {

      List fieldList = (List)fieldLists.get(path);
      Object field = fieldList.get(fieldList.size()-1);
      if(!(field instanceof CMRFieldBridge)) {
         throw new IllegalArgumentException("Path is collection valued: "+path);
      }
      CMRFieldBridge cmrField = (CMRFieldBridge)field;
      if(cmrField.isSingleValued()) {
         throw new IllegalArgumentException("Path is collection valued: "+path);
      }
      identifiers.put(identifier, cmrField.getRelatedEntity());
   }

   public EntityBridge getEntity(String identificationVariable) {
      return (EntityBridge)identifiers.get(identificationVariable);
   }
   
   public void registerPath(
         String path,
         List pathList,
         List fieldList) {

      if(pathList.size() != fieldList.size()) {
         throw new IllegalArgumentException("Path list and field list must " +
               "have the same size: pathList.size=" + pathList.size() +
               " fieldList.size=" + fieldList.size());
      }
      pathLists.put(path, pathList);
      fieldLists.put(path, fieldList);
   }

   public List getPathList(String path) {
      return (List)pathLists.get(path);
   }

   public List getFieldList(String path) {
      return (List)fieldLists.get(path);
   }

}

