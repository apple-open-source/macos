/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.jdbc.bridge;


import java.lang.reflect.Field;
import java.sql.PreparedStatement;
import java.sql.ResultSet;

import org.jboss.ejb.plugins.cmp.bridge.CMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.LockingStrategy;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 * JDBCCMPFieldBridge represents one CMP field. This implementations of 
 * this interface handles setting are responsible for setting statement
 * parameters and loading results for instance values and primary
 * keys.
 *
 * Life-cycle:
 *      Tied to the EntityBridge.
 *
 * Multiplicity:
 *      One for each entity bean cmp field.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 *
 * @version $Revision: 1.11.4.7 $
 */
public interface JDBCCMPFieldBridge extends JDBCFieldBridge, CMPFieldBridge
{
   /**
    * The index of the field among the table fields.
    */
   int getTableIndex();

   /**
    * Returns the default field flags.
    */
   byte getDefaultFlags();

   /**
    * TODO: Get rid of it
    * @param flag
    */
   void addDefaultFlag(byte flag);

   /**
    * @param ctx  instance's context
    * @return  field value that was locked.
    */
   Object getLockedValue(EntityEnterpriseContext ctx);

   /**
    * Optimistically locks field value.
    */
   public void lockInstanceValue(EntityEnterpriseContext ctx);

   /**
    * @param lockingStrategy locking strategy assigned to the field
    */
   void setLockingStrategy(LockingStrategy lockingStrategy);

   /**
    * Gets the field of the primary key object in which the value of this 
    * field is stored.
    */
   public Field getPrimaryKeyField();

   /**
    * Gets the JDBCStoreManager for this field
    */
   public JDBCStoreManager getManager();

   /**
    * Gets the value of this field in the specified primaryKey object.
    * @param primaryKey the primary key object from which this fields value 
    *    will be extracted
    * @return the value of this field in the primaryKey object
    */
   public Object getPrimaryKeyValue(Object primaryKey)
         throws IllegalArgumentException;

   /**
    * @return true if the field belongs to a relation table
    */
   boolean isRelationTableField();

   /**
    * Sets the value of this field to the specified value in the 
    * specified primaryKey object.
    * @param primaryKey the primary key object which the value 
    *    will be inserted
    * @param value the value for field that will be set in the pk
    * @return the updated primary key object; the actual object may 
    *    change not just the value
    */
    public Object setPrimaryKeyValue(Object primaryKey, Object value)
         throws IllegalArgumentException;
   
   /**
    * Sets the prepared statement parameters with the data from the 
    * primary key.
    */
   public int setPrimaryKeyParameters(PreparedStatement ps, int parameterIndex, Object primaryKey) throws IllegalArgumentException;
   
   /**
    * Sets the prepared statement parameters with the data from the 
    * object. The object must be the type of this field.
    */
   public int setArgumentParameters(PreparedStatement ps, int parameterIndex, Object arg);

   /**
    * Loads the data from result set into the primary key object.
    */
   public int loadPrimaryKeyResults(ResultSet rs, int parameterIndex, Object[] pkRef) throws IllegalArgumentException;
}                                         
