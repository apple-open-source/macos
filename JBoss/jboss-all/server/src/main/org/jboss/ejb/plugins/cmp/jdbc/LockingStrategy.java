/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public interface LockingStrategy
{
   LockingStrategy VERSION = new AbstractStrategy()
   {
      public void loaded(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
         field.lockInstanceValue(ctx);
      }
   };

   LockingStrategy GROUP = new AbstractStrategy()
   {
      public void loaded(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
         field.lockInstanceValue(ctx);
      }
   };

   LockingStrategy READ = new AbstractStrategy()
   {
      public void accessed(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
         field.lockInstanceValue(ctx);
      }

      public void changed(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
         field.lockInstanceValue(ctx);
      }
   };

   LockingStrategy MODIFIED = new AbstractStrategy()
   {
      public void changed(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
         field.lockInstanceValue(ctx);
      }
   };

   LockingStrategy NONE = new AbstractStrategy(){};

   class AbstractStrategy implements LockingStrategy
   {
      public void loaded(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
      }

      public void accessed(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
      }

      public void changed(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx)
      {
      }
   }

   void loaded(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx);
   void accessed(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx);
   void changed(JDBCCMPFieldBridge field, EntityEnterpriseContext ctx);
}
