/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.bridge;

import java.util.Collection;
import java.util.List;

/**
 * EntityBridge follows the Bridge pattern [Gamma et. al, 1995].
 * In this implementation of the pattern the Abstract is the entity bean class,
 * and the RefinedAbstraction is the entity bean dynamic proxy. This interface
 * can be considered the implementor. Each imlementation of the CMPStoreManager
 * should create a store specifiec implementaion of the bridge. 
 *
 * Life-cycle:
 *      Undefined. Should be tied to CMPStoreManager.
 *
 * Multiplicity:   
 *      One per cmp entity bean type.       
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.7.2.1 $
 */                            
public interface EntityBridge {
   public String getEntityName();
   public String getAbstractSchemaName();
   
   public FieldBridge getFieldByName(String fieldName);
   public Class getRemoteInterface();
   public Class getLocalInterface();
}
