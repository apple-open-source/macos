/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.bridge;

import org.jboss.ejb.EntityEnterpriseContext;

/**
 * FieldBridge represents one field for one entity. 
 *
 * Life-cycle:
 *      Tied to the EntityBridge.
 *
 * Multiplicity:   
 *      One for each entity bean field.       
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.2 $
 */                            
public interface FieldBridge {
   /**
    * Gets the name of this field.
    * @return the name of this field
    */
   public String getFieldName();
   
   /**
    * Gets the value of this field for the specified instance context.
    * @param ctx the context for which this field's value should be fetched
    * @return the value of this field
    */
   public Object getValue(EntityEnterpriseContext ctx);
      
   /**
    * Sets the value of this field for the specified instance context.
    * @param ctx the context for which this field's value should be set
    * @param value the new value of this field
    */
   public void setValue(EntityEnterpriseContext ctx, Object value);

}
