/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkupdates.util;

import org.jboss.ejb.plugins.AbstractInterceptor;
import org.jboss.ejb.plugins.cmp.ejbql.Catalog;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.invocation.Invocation;

import java.lang.reflect.Method;
import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class DirtyTrackerInterceptor
   extends AbstractInterceptor
{
   // Static

   private static DirtyTrackerInterceptor currentInstance = null;

   public static DirtyTrackerInterceptor getCurrentInstance()
   {
      return currentInstance;
   }

   // Attributes

   private JDBCEntityBridge entityBridge;
   private boolean trackingStarted;
   private List dirtyFields = new ArrayList();

   // Constructor

   public DirtyTrackerInterceptor()
   {
      super();
      currentInstance = this;
   }

   // Public

   public void startTracking()
   {
      trackingStarted = true;
   }

   public void stopTracking()
   {
      trackingStarted = false;
   }

   public List getDirtyFields()
   {
      return dirtyFields;
   }

   public void clearDirtyFields()
   {
      dirtyFields.clear();
   }

   // AbstractInterceptor overrides

   public void start()
   {
      String entityName = container.getBeanMetaData().getEjbName();
      Catalog catalog = (Catalog)container.getEjbModule().getModuleData("CATALOG");
      entityBridge = (JDBCEntityBridge)catalog.getEntityByEJBName(entityName);
   }

   public Object invoke(final Invocation mi)
      throws Exception
   {
      Object result =  getNext().invoke(mi);

      Method method = mi.getMethod();
      if(method == null || !trackingStarted)
         return result;

      EntityEnterpriseContext ctx = (EntityEnterpriseContext)mi.getEnterpriseContext();
      for(Iterator dirtyIter = entityBridge.getDirtyFields(ctx).iterator(); dirtyIter.hasNext();)
      {
         JDBCCMPFieldBridge field = (JDBCCMPFieldBridge)dirtyIter.next();
         dirtyFields.add(field.getFieldName());
      }

      return result;
   }
}
