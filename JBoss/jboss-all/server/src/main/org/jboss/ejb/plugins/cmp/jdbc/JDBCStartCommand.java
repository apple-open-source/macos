/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc;

import java.sql.Connection;
import java.sql.Statement;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import javax.sql.DataSource;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCCMPFieldMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCFunctionMappingMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCRelationshipRoleMetaData;
import org.jboss.logging.Logger;

/**
 * JDBCStartCommand creates the table if specified in xml.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @author <a href="loubyansky@ua.fm">Alex Loubyansky</a>
 * @author <a href="heiko.rupp@cellent.de">Heiko W.Rupp</a>
 * @version $Revision: 1.29.2.21 $
 */
public final class JDBCStartCommand
{
   private static final String IDX_POSTFIX = "_idx";
   private static final String COULDNT_SUSPEND = "Could not suspend current transaction before ";
   private static final String COULDNT_REATTACH = "Could not reattach original transaction after ";
   private final static Object CREATED_TABLES_KEY = new Object();
   private final JDBCStoreManager manager;
   private final JDBCEntityBridge entity;
   private final JDBCEntityMetaData entityMetaData;
   private final Logger log;
   private static int idxCount = 0;

   public JDBCStartCommand(JDBCStoreManager manager)
   {
      this.manager = manager;
      entity = manager.getEntityBridge();
      entityMetaData = entity.getMetaData();

      // Create the Log
      log = Logger.getLogger(
         this.getClass().getName() +
         "." +
         manager.getMetaData().getName());

      // Create the created tables set
      Map applicationData = manager.getApplicationDataMap();
      synchronized(applicationData)
      {
         if(!applicationData.containsKey(CREATED_TABLES_KEY))
         {
            applicationData.put(CREATED_TABLES_KEY, Collections.synchronizedSet(new HashSet()));
         }
      }
   }

   public void execute() throws DeploymentException
   {
      boolean tableExisted = SQLUtil.tableExists(entity.getTableName(), entity.getDataSource());
      entity.setTableExisted(tableExisted);
      // Create table if necessary
      if(!entity.getTableExists())
      {
         if(entityMetaData.getCreateTable())
         {
            DataSource dataSource = entity.getDataSource();
            createTable(dataSource, entity.getTableName(), getEntityCreateTableSQL(dataSource));

            // create indices only if table did not yet exist.
            if(!tableExisted)
            {
               createCMPIndices(dataSource);
            }
            else
            {
               if(log.isDebugEnabled())
                  log.debug("Indices for table " + entity.getTableName() + "not created as table existed");
            }


            // issue extra (user-defined) sql for table
            if(!tableExisted)
            {
               issuePostCreateSQL(dataSource, entity.getTablePostCreateCmd(), entity.getTableName());
            }
            else
            {
               log.debug("Did not issue user-defined SQL for existing table " + entity.getTableName());
            }

         }
         else
         {
            log.debug("Table not create as requested: " + entity.getTableName());
         }
         entity.setTableExists(true);
      }

      // create relation tables
      JDBCCMRFieldBridge[] cmrFields = entity.getCMRFields();
      for(int i = 0; i < cmrFields.length; ++i)
      {
         JDBCCMRFieldBridge cmrField = cmrFields[i];
         JDBCRelationMetaData relationMetaData = cmrField.getRelationMetaData();

         // if the table for the related entity has been created
         if(cmrField.getRelatedJDBCEntity().getTableExists())
         {
            DataSource dataSource = relationMetaData.getDataSource();

            // create the relation table
            if(relationMetaData.isTableMappingStyle() && !relationMetaData.getTableExists())
            {
               if(relationMetaData.getCreateTable())
               {
                  createTable(dataSource, cmrField.getTableName(),
                     getRelationCreateTableSQL(cmrField, dataSource));
               }
               else
               {
                  log.debug("Relation table not created as requested: " + cmrField.getTableName());
               }
               // create Indices if needed
               createCMRIndex(dataSource, cmrField);

               if(relationMetaData.getCreateTable())
               {
                  issuePostCreateSQL(dataSource,
                     cmrField.getRelatedJDBCEntity().getTablePostCreateCmd(),
                     cmrField.getTableName());
               }

            }

            // Only generate indices on foreign key columns if
            // the table was freshly created. If not, we risk
            // creating an index twice and get an exception from the DB
            if(relationMetaData.isForeignKeyMappingStyle() &&
               !cmrField.getRelatedJDBCEntity().getTableExisted())
            {
               createCMRIndex(dataSource, cmrField);
            }
            relationMetaData.setTableExists(true);

            // Create my fk constraint
            addForeignKeyConstraint(cmrField);

            // Create related fk constraint
            if(!entity.equals(cmrField.getRelatedJDBCEntity()))
            {
               addForeignKeyConstraint(cmrField.getRelatedCMRField());
            }
         }
      }
   }

   private void createTable(DataSource dataSource, String tableName, String sql)
      throws DeploymentException
   {
      // does this table already exist
      if(SQLUtil.tableExists(tableName, dataSource))
      {
         log.info("Table '" + tableName + "' already exists");
         return;
      }

      // since we use the pools, we have to do this within a transaction

      // suspend the current transaction
      TransactionManager tm = manager.getContainer().getTransactionManager();
      Transaction oldTransaction;
      try
      {
         oldTransaction = tm.suspend();
      }
      catch(Exception e)
      {
         throw new DeploymentException(COULDNT_SUSPEND +"creating table.", e);
      }

      try
      {
         Connection con = null;
         Statement statement = null;
         try
         {
            // execute sql
            if(log.isDebugEnabled())
               log.debug("Executing SQL: " + sql);

            con = dataSource.getConnection();
            statement = con.createStatement();
            statement.executeUpdate(sql);
         }
         finally
         {
            // make sure to close the connection and statement before
            // comitting the transaction or XA will break
            JDBCUtil.safeClose(statement);
            JDBCUtil.safeClose(con);
         }
      }
      catch(Exception e)
      {
         log.debug("Could not create table " + tableName);
         throw new DeploymentException("Error while creating table " + tableName, e);
      }
      finally
      {
         try
         {
            // resume the old transaction
            if(oldTransaction != null)
            {
               tm.resume(oldTransaction);
            }
         }
         catch(Exception e)
         {
            throw new DeploymentException(COULDNT_REATTACH +  "create table");
         }
      }

      // success
      log.info("Created table '" + tableName + "' successfully.");
      Set createdTables = (Set)manager.getApplicationData(CREATED_TABLES_KEY);
      createdTables.add(tableName);
   }

   /**
    * Create an index on a field. Does the create
    * @param dataSource
    * @param tableName In which table is the index?
    * @param indexName Which is the index?
    * @param sql       The SQL statement to issue
    * @throws DeploymentException
    */
   private void createIndex(DataSource dataSource, String tableName, String indexName, String sql)
      throws DeploymentException
   {
      // we are only called directly after creating a table
      // since we use the pools, we have to do this within a transaction
      // suspend the current transaction
      TransactionManager tm = manager.getContainer().getTransactionManager();
      Transaction oldTransaction;
      try
      {
         oldTransaction = tm.suspend();
      }
      catch(Exception e)
      {
         throw new DeploymentException(COULDNT_SUSPEND + "creating index.", e);
      }

      try
      {
         Connection con = null;
         Statement statement = null;
         try
         {
            // execute sql
            if(log.isDebugEnabled())
               log.debug("Executing SQL: " + sql);
            con = dataSource.getConnection();
            statement = con.createStatement();
            statement.executeUpdate(sql);
         }
         finally
         {
            // make sure to close the connection and statement before
            // comitting the transaction or XA will break
            JDBCUtil.safeClose(statement);
            JDBCUtil.safeClose(con);
         }
      }
      catch(Exception e)
      {
         log.debug("Could not create index " + indexName + "on table" + tableName);
         throw new DeploymentException("Error while creating table", e);
      }
      finally
      {
         try
         {
            // resume the old transaction
            if(oldTransaction != null)
            {
               tm.resume(oldTransaction);
            }
         }
         catch(Exception e)
         {
            throw new DeploymentException(COULDNT_REATTACH + "create index");
         }
      }

      // success
      log.info("Created index '" + indexName + "' on '" + tableName + "' successfully.");
   }


   /**
    * Send (user-defined) SQL commands to the server.
    * The commands can be found in the &lt;sql-statement&gt; elements
    * within the &lt;post-table-create&gt; tag in jbossjdbc-cmp.xml
    * @param dataSource
    */
   private void issuePostCreateSQL(DataSource dataSource, List sql, String table)
      throws DeploymentException
   {
      if(sql == null)
      { // no work to do.
         log.trace("issuePostCreateSQL: sql is null");
         return;
      }

      log.info("issuePostCreateSQL::sql: " + sql.toString() + " on table " + table);

      TransactionManager tm = manager.getContainer().getTransactionManager();
      Transaction oldTransaction;

      try
      {
         oldTransaction = tm.suspend();
      }
      catch(Exception e)
      {
         throw new DeploymentException(COULDNT_SUSPEND + "sending sql command.", e);
      }

      String currentCmd = "";

      try
      {
         Connection con = null;
         Statement statement = null;
         try
         {
            con = dataSource.getConnection();
            statement = con.createStatement();

            // execute sql
            for(int i = 0; i < sql.size(); i++)
            {
               currentCmd = (String)sql.get(i);
               /*
                * Replace %%t in the sql command with the current table name
                */
               currentCmd = replaceTable(currentCmd, table);
               currentCmd = replaceIndexCounter(currentCmd);
               log.info("Executing SQL: " + currentCmd);
               statement.executeUpdate(currentCmd);
            }
         }
         finally
         {
            // make sure to close the connection and statement before
            // comitting the transaction or XA will break
            JDBCUtil.safeClose(statement);
            JDBCUtil.safeClose(con);
         }
      }
      catch(Exception e)
      {
         log.warn("Issuing sql " + currentCmd + " failed: " + e.toString());
         throw new DeploymentException("Error while issuing sql in post-table-create", e);
      }
      finally
      {
         try
         {
            // resume the old transaction
            if(oldTransaction != null)
            {
               tm.resume(oldTransaction);
            }
         }
         catch(Exception e)
         {
            throw new DeploymentException(COULDNT_REATTACH + "create index");
         }
      }

      // success
      log.info("Issued SQL  " + sql + " successfully.");
   }

   private String getEntityCreateTableSQL(DataSource dataSource)
      throws DeploymentException
   {
      StringBuffer sql = new StringBuffer();
      sql.append(SQLUtil.CREATE_TABLE).append(entity.getTableName()).append(" (");

      // add fields
      boolean comma = false;
      JDBCFieldBridge[] fields = entity.getTableFields();
      for(int i = 0; i < fields.length; ++i)
      {
         JDBCFieldBridge field = fields[i];
         JDBCType type = field.getJDBCType();
         if(comma)
            sql.append(SQLUtil.COMMA);
         else
            comma = true;
         addField(type, sql);
      }

      // add a pk constraint
      if(entityMetaData.hasPrimaryKeyConstraint())
      {
         JDBCFunctionMappingMetaData pkConstraint =
            manager.getMetaData().getTypeMapping().getPkConstraintTemplate();
         if(pkConstraint == null)
         {
            throw new IllegalStateException("Primary key constraint is " +
               "not allowed for this type of data source");
         }

         String name = "pk_" + entity.getMetaData().getDefaultTableName();
         name = SQLUtil.fixConstraintName(name, dataSource);
         String[] args = new String[]{
            name,
            SQLUtil.getColumnNamesClause(entity.getPrimaryKeyFields(), new StringBuffer(100)).toString()
         };
         sql.append(SQLUtil.COMMA);
         pkConstraint.getFunctionSql(args, sql);
      }

      return sql.append(')').toString();
   }

   /**
    * Create indices for the fields in the table that have a
    * &lt;dbindex&gt; tag in jbosscmp-jdbc.xml
    * @param dataSource
    * @throws DeploymentException
    */
   private void createCMPIndices(DataSource dataSource)
      throws DeploymentException
   {
      StringBuffer sql;

      // Only create indices on CMP fields
      JDBCCMPFieldBridge[] cmpFields = entity.getTableFields();
      for(int i = 0; i < cmpFields.length; ++i)
      {
         JDBCFieldBridge field = cmpFields[i];
         boolean isIndexed = field.isIndexed();

         if(isIndexed)
         {
            log.debug("Creating index for field " + field.getFieldName());
            sql = new StringBuffer();
            sql.append(SQLUtil.CREATE_INDEX);
            sql.append(entity.getTableName() + IDX_POSTFIX + idxCount);// index name
            sql.append(SQLUtil.ON);
            sql.append(entity.getTableName() + " (");
            SQLUtil.getColumnNamesClause(field, sql);
            sql.append(")");
            
            createIndex(dataSource,
               entity.getTableName(),
               entity.getTableName() + IDX_POSTFIX + idxCount,
               sql.toString());
            idxCount++;
         }
      }
   }

   private void createCMRIndex(DataSource dataSource, JDBCCMRFieldBridge field)
      throws DeploymentException
   {
      JDBCRelationMetaData rmd;
      String tableName;

      rmd = field.getRelationMetaData();

      if(rmd.isTableMappingStyle())
      {
         tableName = rmd.getDefaultTableName();
      }
      else
      {
         tableName = field.getRelatedCMRField().getEntity().getTableName();
      }

      JDBCRelationshipRoleMetaData left, right;

      left = rmd.getLeftRelationshipRole();
      right = rmd.getRightRelationshipRole();

      Collection kfl = left.getKeyFields();
      JDBCCMPFieldMetaData fi;
      Iterator it = kfl.iterator();

      while(it.hasNext())
      {
         fi = (JDBCCMPFieldMetaData)it.next();
         if(left.isIndexed())
         {
            createIndex(dataSource, tableName, fi.getFieldName(), createIndexSQL(fi, tableName));
            idxCount++;
         }
      }

      Collection kfr = right.getKeyFields();
      it = kfr.iterator();
      while(it.hasNext())
      {
         fi = (JDBCCMPFieldMetaData)it.next();
         if(right.isIndexed())
         {
            createIndex(dataSource, tableName, fi.getFieldName(), createIndexSQL(fi, tableName));
            idxCount++;
         }
      }
   }

   private static String createIndexSQL(JDBCCMPFieldMetaData fi, String tableName)
   {
      StringBuffer sql = new StringBuffer();
      sql.append(SQLUtil.CREATE_INDEX);
      sql.append(fi.getColumnName() + IDX_POSTFIX + idxCount);
      sql.append(SQLUtil.ON);
      sql.append(tableName + " (");
      sql.append(fi.getColumnName());
      sql.append(')');
      return sql.toString();
   }

   private void addField(JDBCType type, StringBuffer sqlBuffer)
   {
      // apply auto-increment template
      if(type.getAutoIncrement()[0])
      {
         String columnClause = SQLUtil.getCreateTableColumnsClause(type);
         JDBCFunctionMappingMetaData autoIncrement =
            manager.getMetaData().getTypeMapping().getAutoIncrementTemplate();
         if(autoIncrement == null)
         {
            throw new IllegalStateException("auto-increment template not found");
         }
         String[] args = new String[]{columnClause};
         autoIncrement.getFunctionSql(args, sqlBuffer);
      }
      else
      {
         sqlBuffer.append(SQLUtil.getCreateTableColumnsClause(type));
      }
   }

   private String getRelationCreateTableSQL(
      JDBCCMRFieldBridge cmrField,
      DataSource dataSource) throws DeploymentException
   {
      JDBCCMPFieldBridge[] leftKeys = cmrField.getTableKeyFields();
      JDBCCMPFieldBridge[] rightKeys = cmrField.getRelatedCMRField().getTableKeyFields();
      JDBCFieldBridge[] fieldsArr = new JDBCFieldBridge[leftKeys.length + rightKeys.length];
      System.arraycopy(leftKeys, 0, fieldsArr, 0, leftKeys.length);
      System.arraycopy(rightKeys, 0, fieldsArr, leftKeys.length, rightKeys.length);

      StringBuffer sql = new StringBuffer();
      sql.append(SQLUtil.CREATE_TABLE).append(cmrField.getTableName())
         .append(" (")
         // add field declaration
         .append(SQLUtil.getCreateTableColumnsClause(fieldsArr));

      // add a pk constraint
      if(cmrField.getRelationMetaData().hasPrimaryKeyConstraint())
      {
         JDBCFunctionMappingMetaData pkConstraint =
            manager.getMetaData().getTypeMapping().getPkConstraintTemplate();
         if(pkConstraint == null)
         {
            throw new IllegalStateException(
               "Primary key constraint is not allowed for this type of data store");
         }

         String name = "pk_" + cmrField.getRelationMetaData().getDefaultTableName();
         name = SQLUtil.fixConstraintName(name, dataSource);
         String[] args = new String[]{
            name,
            SQLUtil.getColumnNamesClause(
               fieldsArr, new StringBuffer(100).toString(), new StringBuffer()).toString()
         };
         sql.append(SQLUtil.COMMA);
         pkConstraint.getFunctionSql(args, sql);
      }
      sql.append(')');
      return sql.toString();
   }

   private void addForeignKeyConstraint(JDBCCMRFieldBridge cmrField)
      throws DeploymentException
   {
      if(cmrField.getMetaData().hasForeignKeyConstraint())
      {
         if(cmrField.getRelationMetaData().isTableMappingStyle())
         {
            addForeignKeyConstraint(
               cmrField.getRelationMetaData().getDataSource(),
               cmrField.getTableName(),
               cmrField.getFieldName(),
               cmrField.getTableKeyFields(),
               cmrField.getEntity().getTableName(),
               cmrField.getEntity().getPrimaryKeyFields());

         }
         else if(cmrField.hasForeignKey())
         {
            addForeignKeyConstraint(
               cmrField.getEntity().getDataSource(),
               cmrField.getEntity().getTableName(),
               cmrField.getFieldName(),
               cmrField.getForeignKeyFields(),
               cmrField.getRelatedJDBCEntity().getTableName(),
               cmrField.getRelatedJDBCEntity().getPrimaryKeyFields());
         }
      }
      else
      {
         log.debug("Foreign key constraint not added as requested: relationshipRolename=" +
            cmrField.getMetaData().getRelationshipRoleName());
      }
   }

   private void addForeignKeyConstraint(
      DataSource dataSource,
      String tableName,
      String cmrFieldName,
      JDBCCMPFieldBridge[] fields,
      String referencesTableName,
      JDBCCMPFieldBridge[] referencesFields) throws DeploymentException
   {
      // can only alter tables we created
      Set createdTables = (Set)manager.getApplicationData(CREATED_TABLES_KEY);
      if(!createdTables.contains(tableName))
      {
         return;
      }

      JDBCFunctionMappingMetaData fkConstraint =
         manager.getMetaData().getTypeMapping().getFkConstraintTemplate();
      if(fkConstraint == null)
      {
         throw new IllegalStateException("Foreign key constraint is not " +
            "allowed for this type of datastore");
      }
      String a = SQLUtil.getColumnNamesClause(fields, new StringBuffer(50)).toString();
      String b = SQLUtil.getColumnNamesClause(referencesFields, new StringBuffer(50)).toString();

      String[] args = new String[]{
         tableName,
         SQLUtil.fixConstraintName(
            "fk_" + tableName + "_" + cmrFieldName, dataSource),
         a,
         referencesTableName,
         b};

      String sql = fkConstraint.getFunctionSql(args, new StringBuffer(100)).toString();

      // since we use the pools, we have to do this within a transaction
      // suspend the current transaction
      TransactionManager tm = manager.getContainer().getTransactionManager();
      Transaction oldTransaction;
      try
      {
         oldTransaction = tm.suspend();
      }
      catch(Exception e)
      {
         throw new DeploymentException(COULDNT_SUSPEND + "alter table create foreign key.", e);
      }

      try
      {
         Connection con = null;
         Statement statement = null;
         try
         {
            if(log.isDebugEnabled())
               log.debug("Executing SQL: " + sql);
            con = dataSource.getConnection();
            statement = con.createStatement();
            statement.executeUpdate(sql);
         }
         finally
         {
            // make sure to close the connection and statement before
            // comitting the transaction or XA will break
            JDBCUtil.safeClose(statement);
            JDBCUtil.safeClose(con);
         }
      }
      catch(Exception e)
      {
         log.warn("Could not add foreign key constraint: table=" + tableName);
         throw new DeploymentException("Error while adding foreign key " +
            "constraint", e);
      }
      finally
      {
         try
         {
            // resume the old transaction
            if(oldTransaction != null)
            {
               tm.resume(oldTransaction);
            }
         }
         catch(Exception e)
         {
            throw new DeploymentException(COULDNT_REATTACH + "create table");
         }
      }

      // success
      log.info("Added foreign key constraint to table '" + tableName + "'");
   }


   /**
    * Replace %%t in the sql command with the current table name
    *
    * @param in sql statement with possible %%t to substitute with table name
    * @param table the table name
    * @return String with sql statement
    */
   private static String replaceTable(String in, String table)
   {
      int pos;

      pos = in.indexOf("%%t");
      // No %%t -> return input
      if(pos == -1)
         return in;

      String first = in.substring(0, pos);
      String last = in.substring(pos + 3);

      return first + table + last;
   }

   /**
    * Replace %%n in the sql command with a running (index) number
    * @param in
    * @return
    */
   private static String replaceIndexCounter(String in)
   {
      int pos;

      pos = in.indexOf("%%n");
      // No %%n -> return input
      if(pos == -1)
         return in;

      String first = in.substring(0, pos);
      String last = in.substring(pos + 3);
      idxCount++;
      return first + idxCount + last;
   }
}
