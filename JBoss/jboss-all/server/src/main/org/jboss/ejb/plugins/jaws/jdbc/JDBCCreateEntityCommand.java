/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

import java.util.Iterator;

import java.sql.PreparedStatement;

import javax.ejb.CreateException;
import javax.ejb.DuplicateKeyException;
import javax.ejb.EJBException;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.jaws.JAWSPersistenceManager;
import org.jboss.ejb.plugins.jaws.JPMCreateEntityCommand;

import org.jboss.ejb.plugins.jaws.metadata.CMPFieldMetaData;
import org.jboss.ejb.plugins.jaws.metadata.PkFieldMetaData;

import org.jboss.logging.Logger;

/**
 * JAWSPersistenceManager JDBCCreateEntityCommand
 *
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.12 $
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   </ul>
 *
 */
public class JDBCCreateEntityCommand
   extends JDBCUpdateCommand
   implements JPMCreateEntityCommand
{
   // Attributes ----------------------------------------------------
   
   private JDBCBeanExistsCommand beanExistsCommand;
   private Logger log = Logger.getLogger(JDBCCreateEntityCommand.class);

   // Constructors --------------------------------------------------
   
   public JDBCCreateEntityCommand(JDBCCommandFactory factory)
   {
      super(factory, "Create");
      
      beanExistsCommand = factory.createBeanExistsCommand();
      
      // Insert SQL
      
      String sql = "INSERT INTO " + jawsEntity.getTableName();
      String fieldSql = "";
      String valueSql = "";
      
      Iterator it = jawsEntity.getCMPFields();
      boolean first = true;
      
      while (it.hasNext())
      {
         CMPFieldMetaData cmpField = (CMPFieldMetaData)it.next();
         
         fieldSql += (first ? "" : ",") +
                     cmpField.getColumnName();
         valueSql += first ? "?" : ",?";
         first = false;
      }
      
      sql += " ("+fieldSql+") VALUES ("+valueSql+")";
      
      setSQL(sql);
   }
   
   // JPMCreateEntityCommand implementation -------------------------
   
   public Object execute(Method m,
                       Object[] args,
                       EntityEnterpriseContext ctx)
      throws CreateException
   {
      try
      {
         // Extract pk
         Object id = null;
         Iterator it = jawsEntity.getPkFields();
         
         if (jawsEntity.hasCompositeKey())
         {
            try
            {
               id = jawsEntity.getPrimaryKeyClass().newInstance();
            } catch (InstantiationException e)
            {
               throw new EJBException("Could not create primary key",e);
            }
            
            while (it.hasNext())
            {
               PkFieldMetaData pkField = (PkFieldMetaData)it.next();
               Field from = pkField.getCMPField();
               Field to = pkField.getPkField();
               to.set(id, from.get(ctx.getInstance()));
            }
         } else
         {
            PkFieldMetaData pkField = (PkFieldMetaData)it.next();
            Field from = pkField.getCMPField();
            id = from.get(ctx.getInstance());
         }
         
         if (log.isDebugEnabled())
         {
            log.debug("Create, id is "+id);
         }
         
         // Check duplicate
         if (beanExistsCommand.execute(id))
         {
            throw new DuplicateKeyException("Entity with key "+id+" already exists");
         }
         
         // Insert in db
         
         try
         {
            jdbcExecute(ctx);
         } catch (Exception e)
         {
            if (log.isDebugEnabled())
               log.debug("Exception", e);
            throw new CreateException("Could not create entity:"+e);
         }
         
         return id;
         
      } catch (IllegalAccessException e)
      {
         if (log.isDebugEnabled())
            log.debug("IllegalAccessException", e);
         throw new CreateException("Could not create entity:"+e);
      }
   }
   
   // JDBCUpdateCommand overrides -----------------------------------
   
   protected void setParameters(PreparedStatement stmt, Object argOrArgs) 
      throws Exception
   {
      EntityEnterpriseContext ctx = (EntityEnterpriseContext)argOrArgs;
      int idx = 1; // Parameter-index
      
      Iterator iter = jawsEntity.getCMPFields();
      while (iter.hasNext())
      {
         CMPFieldMetaData cmpField = (CMPFieldMetaData)iter.next();
         Object value = getCMPFieldValue(ctx.getInstance(), cmpField);
         
         setParameter(stmt, idx++, cmpField.getJDBCType(), value);
      }
   }
   
   protected Object handleResult(int rowsAffected, Object argOrArgs) 
      throws Exception
   {
      // arguably should check one row went in!!!
      
      EntityEnterpriseContext ctx = (EntityEnterpriseContext)argOrArgs;
      
      // Store state to be able to do tuned updates
      JAWSPersistenceManager.PersistenceContext pCtx =
         new JAWSPersistenceManager.PersistenceContext();
      
      // If read-only, set last read to now
      if (jawsEntity.isReadOnly()) pCtx.lastRead = System.currentTimeMillis();
      
      // Save initial state for tuned updates
      pCtx.state = getState(ctx);
      
      ctx.setPersistenceContext(pCtx);
      
      return null;
   }
}
