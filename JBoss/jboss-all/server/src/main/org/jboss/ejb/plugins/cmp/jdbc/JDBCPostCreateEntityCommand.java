/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;
import java.util.Iterator;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalObject;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;

/**
 * This command establishes relationships for CMR fields that have
 * foreign keys mapped to primary keys.
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 *
 * @version $Revision: 1.2.2.6 $
 */
public class JDBCPostCreateEntityCommand
{
   // Attributes ------------------------------------
   private JDBCEntityBridge entity;

   // Constructors ----------------------------------
   public JDBCPostCreateEntityCommand(JDBCStoreManager manager)
   {
      entity = manager.getEntityBridge();
   }

   // Public ----------------------------------------
   public Object execute(Method m,
                         Object[] args,
                         EntityEnterpriseContext ctx)
      throws CreateException
   {
      for(Iterator cmrFieldsIter = entity.getCMRFields().iterator(); cmrFieldsIter.hasNext();)
      {
         JDBCCMRFieldBridge cmrField = (JDBCCMRFieldBridge)cmrFieldsIter.next();
         if(cmrField.hasFKFieldsMappedToCMPFields())
         {
            Object relatedId = cmrField.getRelatedIdFromContextCMP(ctx);
            if(relatedId != null)
            {
               try
               {
                  EJBLocalObject relatedEntity = cmrField.getRelatedEntityByFK(relatedId, ctx);
                  if(relatedEntity != null)
                  {
                     cmrField.createRelationLinks(ctx, relatedId);
                  }
                  else
                  {
                     cmrField.getRelatedCMRField().addRelatedPKWaitingForMyPK(relatedId, ctx.getId());
                  }
               }
               catch(Exception e)
               {
                  // no such object
               }
            }
         }
         else if(cmrField.getRelatedCMRField().hasFKFieldsMappedToCMPFields())
         {
            cmrField.addRelatedPKsWaitedForMe(ctx);
         }
      }
      return null;
   }
}
