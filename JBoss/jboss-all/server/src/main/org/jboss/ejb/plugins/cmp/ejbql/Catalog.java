/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.jboss.ejb.plugins.cmp.bridge.EntityBridge;

/**
 * This class maintains a map of all entitie bridges in an application by name.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.4.2.1 $
 */                            
public final class Catalog {
   private final Map entityByAbstractSchemaName = new HashMap();
   private final Map entityByEJBName = new HashMap();
   private final Map entityByInterface = new HashMap();

   public void addEntity(EntityBridge entityBridge) {
      entityByAbstractSchemaName.put(
            entityBridge.getAbstractSchemaName(), 
            entityBridge);
      entityByEJBName.put(
            entityBridge.getEntityName(), 
            entityBridge);

      Class remote = entityBridge.getRemoteInterface();
      if(remote != null) {
         entityByInterface.put(remote, entityBridge);
      }
      Class local = entityBridge.getLocalInterface();
      if(local != null) {
         entityByInterface.put(local, entityBridge);
      }
         
   }

   public EntityBridge getEntityByAbstractSchemaName(
         String abstractSchemaName) {
      return (EntityBridge) entityByAbstractSchemaName.get(abstractSchemaName);
   }

   public EntityBridge getEntityByInterface(Class intf) {
      return (EntityBridge) entityByInterface.get(intf);
   }

   public EntityBridge getEntityByEJBName(String ejbName) {
      return (EntityBridge) entityByEJBName.get(ejbName);
   }

   public int getEntityCount() {
      return entityByEJBName.size();
   }

   public Set getEJBNames() {
      return Collections.unmodifiableSet(entityByEJBName.keySet());
   }
}
