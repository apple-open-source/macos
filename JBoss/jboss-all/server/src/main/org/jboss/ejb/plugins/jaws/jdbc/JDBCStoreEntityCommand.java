/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.sql.PreparedStatement;
import java.util.Iterator;
import javax.ejb.EJBException;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.jaws.JAWSPersistenceManager;
import org.jboss.ejb.plugins.jaws.JPMStoreEntityCommand;
import org.jboss.ejb.plugins.jaws.metadata.CMPFieldMetaData;

/**
 * JAWSPersistenceManager JDBCStoreEntityCommand
 *
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @version $Revision: 1.10.2.1 $
 */
public class JDBCStoreEntityCommand
   extends JDBCUpdateCommand
   implements JPMStoreEntityCommand
{
   // Constructors --------------------------------------------------
   
   public JDBCStoreEntityCommand(JDBCCommandFactory factory)
   {
      super(factory, "Store");
      boolean tuned = jawsEntity.hasTunedUpdates();
      
      // If we don't have tuned updates, create static SQL
      if (!tuned)
      {
         setSQL(makeSQL(null));
      }
   }
   
   // JPMStoreEntityCommand implementation ---------------------------
   
   /**
    * if the readOnly flag is specified in the xml file this won't store.
    * if not a tuned or untuned update is issued.
    */
   public void execute(EntityEnterpriseContext ctx)
   {
      // Check for read-only
      // JF: Shouldn't this throw an exception?
      if (jawsEntity.isReadOnly())
      {
         return;
      }
      
      ExecutionState es = new ExecutionState();
      es.ctx = ctx;
      es.currentState = getState(ctx);
      boolean dirty = false;
      
      
      boolean tuned = jawsEntity.hasTunedUpdates();
      
      // For tuned updates, need to see which fields have changed
      
      if (tuned)
      {
         es.dirtyField = new boolean[es.currentState.length];
         Object[] oldState =
            ((JAWSPersistenceManager.PersistenceContext)ctx.getPersistenceContext()).state;
         
         for (int i = 0; i < es.currentState.length; i++)
         {
            es.dirtyField[i] = changed(es.currentState[i], oldState[i]);
            dirty |= es.dirtyField[i];
         }
      }
      
      if (!tuned || dirty)
      {
         try
         {
            // Update db
            jdbcExecute(es);
            
         } catch (Exception e)
         {
            throw new EJBException("Store failed", e);
         }
      }
   }
   
   // JDBCUpdateCommand overrides -----------------------------------
   
   /**
    * Returns dynamically-generated SQL if this entity
    * has tuned updates, otherwise static SQL.
    */
   protected String getSQL(Object argOrArgs) throws Exception
   {
      boolean tuned = jawsEntity.hasTunedUpdates();
      
      return tuned ? makeSQL(argOrArgs) : super.getSQL(argOrArgs);
   }
   
   protected void setParameters(PreparedStatement stmt, Object argOrArgs) 
      throws Exception
   {
      ExecutionState es = (ExecutionState)argOrArgs;
      boolean tuned = jawsEntity.hasTunedUpdates();
      
      int idx = 1;
      Iterator iter = jawsEntity.getCMPFields();
      int i = 0;
      while (iter.hasNext())
      {
         CMPFieldMetaData cmpField = (CMPFieldMetaData)iter.next();
         
         if (!tuned || es.dirtyField[i])
         {
            setParameter(stmt, idx++, cmpField.getJDBCType(), es.currentState[i]);
         }
         
         i++;
      }
      
      setPrimaryKeyParameters(stmt, idx, es.ctx.getId());
   }
   
   protected Object handleResult(int rowsAffected, Object argOrArgs) 
      throws Exception
   {
      ExecutionState es = (ExecutionState)argOrArgs;
      boolean tuned = jawsEntity.hasTunedUpdates();
      
      if (tuned)
      {
         // Save current state for tuned updates
         JAWSPersistenceManager.PersistenceContext pCtx =
            (JAWSPersistenceManager.PersistenceContext)es.ctx.getPersistenceContext();
         pCtx.state = es.currentState;
      }
      
      return null;
   }
   
   // Protected -----------------------------------------------------
   
   protected final boolean changed(Object current, Object old)
   {
      return (current == null) ? (old != null) : (old == null ? true : !current.equals(old));
   }
   
   /** 
    * Used to create static SQL (tuned = false) or dynamic SQL (tuned = true).
    */
   protected String makeSQL(Object argOrArgs)
   {
      ExecutionState es = (ExecutionState)argOrArgs;  // NB: null if tuned
      boolean tuned = jawsEntity.hasTunedUpdates();

      StringBuffer sb = new StringBuffer(200);
      sb.append("UPDATE ");
      sb.append(jawsEntity.getTableName());
      sb.append(" SET ");
      Iterator iter = jawsEntity.getCMPFields();
      int i = 0;
      boolean first = true;
      while (iter.hasNext())
      {
         CMPFieldMetaData cmpField = (CMPFieldMetaData)iter.next();
         
         if (!tuned || es.dirtyField[i++])
         {
             if(first) {
                 first=false;
             }else {
                 sb.append(',');
             }
             sb.append(cmpField.getColumnName());
             sb.append("=?");
         }
      }
      sb.append(" WHERE ");
      sb.append(getPkColumnWhereList());
      return sb.toString();
   }
   
   // Inner Classes -------------------------------------------------
   
   protected static class ExecutionState
   {
      public EntityEnterpriseContext ctx;
      public Object[] currentState;
      public boolean[] dirtyField;    // only used for tuned updates
   }
}
