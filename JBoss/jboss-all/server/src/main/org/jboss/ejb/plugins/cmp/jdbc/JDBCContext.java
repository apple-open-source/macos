/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;


/**
 *
 * @author <a href="alex@jboss.org">Alex Loubyansky</a>
 */
public final class JDBCContext
{
   private final Object[] fieldStates;
   private JDBCEntityBridge.EntityState entityState;

   public JDBCContext(int jdbcContextSize, JDBCEntityBridge.EntityState entityState)
   {
      fieldStates = new Object[jdbcContextSize];
      this.entityState = entityState;
   }

   public Object getFieldState(int index)
   {
      return fieldStates[index];
   }

   public void setFieldState(int index, Object value)
   {
      fieldStates[index] = value;
   }

   public JDBCEntityBridge.EntityState getEntityState()
   {
      return entityState;
   }
}
