/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.math.BigDecimal;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;

import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectInputStream;
import java.io.IOException;
import java.io.Reader;
import java.io.BufferedReader;

import java.sql.Blob;
import java.sql.Clob;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Types;

import java.rmi.RemoteException;
import java.rmi.MarshalledObject;

import javax.ejb.EJBObject;
import javax.ejb.Handle;

import javax.sql.DataSource;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.jaws.metadata.JawsEntityMetaData;
import org.jboss.ejb.plugins.jaws.metadata.CMPFieldMetaData;
import org.jboss.ejb.plugins.jaws.metadata.PkFieldMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.ByteArrayBlob;
import org.jboss.logging.Logger;

/**
 * Abstract superclass for all JAWS Commands that use JDBC directly.
 * Provides a Template Method for jdbcExecute(), default implementations
 * for some of the methods called by this template, and a bunch of
 * utility methods that database commands may need to call.
 *
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:dirk@jboss.de">Dirk Zimmermann</a>
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Revision: 1.43.2.3 $ 
 * 
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010621 danch:</b>
 *   <ul>
 *   <li>add getter for name
 *   </ul>
 *
 *   <p><b>20010621 (ref 1.25) danch:</b>
 *   <ul>
 *   <li>improve logging: an exception execing SQL is an error!
 *   </ul>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   </ul>
 *
 */
public abstract class JDBCCommand
{
    private final static HashMap rsTypes = new HashMap();
    static {
        Class[] arg = new Class[]{Integer.TYPE};
        try {
            rsTypes.put(java.util.Date.class.getName(),       ResultSet.class.getMethod("getTimestamp", arg));
            rsTypes.put(java.sql.Date.class.getName(),        ResultSet.class.getMethod("getDate", arg));
            rsTypes.put(java.sql.Time.class.getName(),        ResultSet.class.getMethod("getTime", arg));
            rsTypes.put(java.sql.Timestamp.class.getName(),   ResultSet.class.getMethod("getTimestamp", arg));
            rsTypes.put(java.math.BigDecimal.class.getName(), ResultSet.class.getMethod("getBigDecimal", arg));
            rsTypes.put(java.sql.Ref.class.getName(),         ResultSet.class.getMethod("getRef", arg));
            rsTypes.put(java.lang.String.class.getName(),     ResultSet.class.getMethod("getString", arg));
            rsTypes.put(java.lang.Boolean.class.getName(),    ResultSet.class.getMethod("getBoolean", arg));
            rsTypes.put(Boolean.TYPE.getName(),               ResultSet.class.getMethod("getBoolean", arg));
            rsTypes.put(java.lang.Byte.class.getName(),       ResultSet.class.getMethod("getByte", arg));
            rsTypes.put(Byte.TYPE.getName(),                  ResultSet.class.getMethod("getByte", arg));
            rsTypes.put(java.lang.Double.class.getName(),     ResultSet.class.getMethod("getDouble", arg));
            rsTypes.put(Double.TYPE.getName(),                ResultSet.class.getMethod("getDouble", arg));
            rsTypes.put(java.lang.Float.class.getName(),      ResultSet.class.getMethod("getFloat", arg));
            rsTypes.put(Float.TYPE.getName(),                 ResultSet.class.getMethod("getFloat", arg));
            rsTypes.put(java.lang.Integer.class.getName(),    ResultSet.class.getMethod("getInt", arg));
            rsTypes.put(Integer.TYPE.getName(),               ResultSet.class.getMethod("getInt", arg));
            rsTypes.put(java.lang.Long.class.getName(),       ResultSet.class.getMethod("getLong", arg));
            rsTypes.put(Long.TYPE.getName(),                  ResultSet.class.getMethod("getLong", arg));
            rsTypes.put(java.lang.Short.class.getName(),      ResultSet.class.getMethod("getShort", arg));
            rsTypes.put(Short.TYPE.getName(),                 ResultSet.class.getMethod("getShort", arg));
        } catch(NoSuchMethodException e) {
            Logger.getLogger(JDBCCommand.class).error("NoSuchMethodException", e);
        }
    }

   // Attributes ----------------------------------------------------

   protected JDBCCommandFactory factory;
   protected JawsEntityMetaData jawsEntity;
   protected String name;    // Command name, used for debug trace

   private Logger log = Logger.getLogger(JDBCCommand.class);
   private String sql;
   private static Map jdbcTypeNames;

   // Constructors --------------------------------------------------

   /**
    * Construct a JDBCCommand with given factory and name.
    *
    * @param factory the factory which was used to create this JDBCCommand,
    *  which is also used as a common repository, shared by all an
    *  entity's Commands.
    * @param name the name to be used when tracing execution.
    */
   protected JDBCCommand(JDBCCommandFactory factory, String name)
   {
      this.factory = factory;
      this.jawsEntity = factory.getMetaData();
      this.name = name;
   }

   /** return the name of this command */
   public String getName() {
      return name;
   }
   
   // Protected -----------------------------------------------------

   /**
    * Template method handling the mundane business of opening
    * a database connection, preparing a statement, setting its parameters,
    * executing the prepared statement, handling the result,
    * and cleaning up.
    *
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method, and passed on to 'hook' methods for
    *  getting SQL and for setting parameters.
    * @return any result produced by the handling of the result of executing
    *  the prepared statement.
    * @throws Exception if connection fails, or if any 'hook' method
    *  throws an exception.
    */
   protected Object jdbcExecute(Object argOrArgs) throws Exception
   {
      Connection con = null;
      PreparedStatement stmt = null;
      Object result = null;

      try
      {
         con = getConnection();
         String theSQL = getSQL(argOrArgs);
         if (log.isDebugEnabled())
         {
            log.debug(name + " command executing: " + theSQL);
         }
         stmt = con.prepareStatement(theSQL);
         setParameters(stmt, argOrArgs);
         result = executeStatementAndHandleResult(stmt, argOrArgs);
      } catch(SQLException e) {
          log.error("Exception caught executing SQL", e);
          throw e;
      } finally
      {
         if (stmt != null)
         {
            try
            {
               stmt.close();
            } catch (SQLException e)
            {
               log.error("SQLException", e);
            }
         }
         if (con != null)
         {
            try
            {
               con.close();
            } catch (SQLException e)
            {
               log.error("SQLException", e);
            }
         }
      }

      return result;
   }

   /**
    * Used to set static SQL in subclass constructors.
    *
    * @param sql the static SQL to be used by this Command.
    */
   protected void setSQL(String sql)
   {
      if (log.isDebugEnabled())
         log.debug(name + " SQL: " + sql);
      this.sql = sql;
   }

   /**
    * Gets the SQL to be used in the PreparedStatement.
    * The default implementation returns the <code>sql</code> field value.
    * This is appropriate in all cases where static SQL can be
    * constructed in the Command constructor.
    * Override if dynamically-generated SQL, based on the arguments
    * given to execute(), is needed.
    *
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method.
    * @return the SQL to use in the PreparedStatement.
    * @throws Exception if an attempt to generate dynamic SQL results in
    *  an Exception.
    */
   protected String getSQL(Object argOrArgs) throws Exception
   {
      return sql;
   }

   /**
    * Default implementation does nothing.
    * Override if parameters need to be set.
    *
    * @param stmt the PreparedStatement which will be executed by this Command.
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method.
    * @throws Exception if parameter setting fails.
    */
   protected void setParameters(PreparedStatement stmt, Object argOrArgs)
      throws Exception
   {
   }

   /**
    * Executes the PreparedStatement and handles result of successful execution.
    * This is implemented in subclasses for queries and updates.
    *
    * @param stmt the PreparedStatement to execute.
    * @param argOrArgs argument or array of arguments passed in from
    *  subclass execute method.
    * @return any result produced by the handling of the result of executing
    *  the prepared statement.
    * @throws Exception if execution or result handling fails.
    */
   protected abstract Object executeStatementAndHandleResult(
            PreparedStatement stmt,
            Object argOrArgs) throws Exception;

   // ---------- Utility methods for use in subclasses ----------

   /**
    * Sets a parameter in this Command's PreparedStatement.
    * Handles null values, and provides tracing.
    *
    * @param stmt the PreparedStatement whose parameter needs to be set.
    * @param idx the index (1-based) of the parameter to be set.
    * @param jdbcType the JDBC type of the parameter.
    * @param value the value which the parameter is to be set to.
    * @throws SQLException if parameter setting fails.
    */
   protected void setParameter(PreparedStatement stmt,
                               int idx,
                               int jdbcType,
                               Object value)
      throws SQLException
   {
      if (log.isDebugEnabled())
      {
         log.debug("Set parameter: idx=" + idx +
                   ", jdbcType=" + getJDBCTypeName(jdbcType) +
                   ", value=" +
                   ((value == null) ? "NULL" : value));
      }

      if (value == null) {
         stmt.setNull(idx, jdbcType);
      } else {
          if(jdbcType == Types.DATE) {
              if(value.getClass().getName().equals("java.util.Date"))
                  value = new java.sql.Date(((java.util.Date)value).getTime());
          } else if(jdbcType == Types.TIME) {
              if(value.getClass().getName().equals("java.util.Date"))
                  value = new java.sql.Time(((java.util.Date)value).getTime());
          } else if(jdbcType == Types.TIMESTAMP) {
              if(value.getClass().getName().equals("java.util.Date"))
                  value = new java.sql.Timestamp(((java.util.Date)value).getTime());
          }
          if (isBinaryType(jdbcType)) {
              byte[] bytes = null;
              if (value instanceof byte[]) {
                  bytes = (byte[])value;
              } else {
                  // ejb-reference: store the handle
                  if (value instanceof EJBObject) try {
                      value = ((EJBObject)value).getHandle();
                  } catch (RemoteException e) {
                      throw new SQLException
                          ("Cannot get Handle of EJBObject: "+e);
                  }

                  try {
                      ByteArrayOutputStream baos = new ByteArrayOutputStream();
                      ObjectOutputStream oos = new ObjectOutputStream(baos);
                      oos.writeObject(new MarshalledObject(value));
                      bytes = baos.toByteArray();
                      oos.close();
                  } catch (IOException e) {
                      throw new SQLException
                          ("Can't serialize binary object: " + e);
                  }
              }

              // it's more efficient to use setBinaryStream for large
              // streams, and causes problems if not done on some DBMS
              // implementations
              // if this is an Oracle Blob only setBlob will work.
              if (jdbcType == Types.BLOB) {
                 stmt.setBlob(idx, new ByteArrayBlob(bytes));
              } else if (value instanceof java.math.BigInteger) {
                 stmt.setObject(idx, ((java.math.BigInteger)value), jdbcType);
              } else if (bytes.length < 2000) {
                 stmt.setBytes(idx, bytes);
              } else {
                  try {
                      ByteArrayInputStream bais =
                          new ByteArrayInputStream(bytes);
                      stmt.setBinaryStream(idx, bais, bytes.length);
                      bais.close();
                  } catch (IOException e) {
                      throw new SQLException
                          ("Couldn't write binary object to DB: " + e);
                  }
              }
          } else {
             switch(jdbcType)
             {
                case Types.DECIMAL:
                case Types.NUMERIC:
                   if(value instanceof BigDecimal)
                   {
                      stmt.setBigDecimal(idx, (BigDecimal)value);
                   }
                   else
                   {
                      stmt.setObject(idx, value, jdbcType, 0);
                   }
                   break;
                default:
                   stmt.setObject(idx, value, jdbcType);
             }
          }
      }
   }

   /**
    * Sets the PreparedStatement parameters for a primary key
    * in a WHERE clause.
    *
    * @param stmt the PreparedStatement
    * @param parameterIndex the index (1-based) of the first parameter to set
    * in the PreparedStatement
    * @param id the entity's ID
    * @return the index of the next unset parameter
    * @throws SQLException if parameter setting fails
    * @throws IllegalAccessException if accessing a field in the PK class fails
    */
   protected int setPrimaryKeyParameters(PreparedStatement stmt,
                                         int parameterIndex,
                                         Object id)
      throws IllegalAccessException, SQLException
   {
      Iterator it = jawsEntity.getPkFields();

      if (jawsEntity.hasCompositeKey())
      {
         while (it.hasNext())
         {
            PkFieldMetaData pkFieldMetaData = (PkFieldMetaData)it.next();
            int jdbcType = pkFieldMetaData.getJDBCType();
            Object value = getPkFieldValue(id, pkFieldMetaData);
            setParameter(stmt, parameterIndex++, jdbcType, value);
         }
      } else
      {
         PkFieldMetaData pkFieldMetaData = (PkFieldMetaData)it.next();
         int jdbcType = pkFieldMetaData.getJDBCType();
         setParameter(stmt, parameterIndex++, jdbcType, id);
      }

      return parameterIndex;
   }


   /**
    * Used for all retrieval of results from <code>ResultSet</code>s.
    * Implements tracing, and allows some tweaking of returned types.
    *
    * @param rs the <code>ResultSet</code> from which a result is being retrieved.
    * @param idx index of the result column.
    * @param destination The class of the variable this is going into
    */
    protected Object getResultObject(ResultSet rs, int idx, Class destination)
        throws SQLException{

// log.debug("getting a "+destination.getName()+" from resultset at index "+idx);
        Object result = null;

        Method method = (Method)rsTypes.get(destination.getName());
        if(method != null) {
            try {
                result = method.invoke(rs, new Object[]{new Integer(idx)});
                if(rs.wasNull()) return null;
                return result;
            } catch(IllegalAccessException e) {
                log.debug("Unable to read from ResultSet: ",e);
            } catch(InvocationTargetException e) {
                log.debug("Unable to read from ResultSet: ",e);
            }
        }

        result = rs.getObject(idx);
        if(result == null)
            return null;

         if(destination == java.math.BigInteger.class
            && result.getClass() == java.math.BigDecimal.class) {
            return ((java.math.BigDecimal)result).toBigInteger();
         }

        if(destination.isAssignableFrom(result.getClass()) && !result.getClass().equals(MarshalledObject.class) )
            return result;
        else if(log.isDebugEnabled())
            log.debug("Got a "+result.getClass().getName()+": '"+result+"' while looking for a "+destination.getName());

        // Also we should detect the EJB references here

        // Get the underlying byte[]

        byte[] bytes = null;
        if(result instanceof byte[]) {
            bytes = (byte[])result;
        } else if(result instanceof Blob) {
            Blob blob = (Blob)result;
            bytes = blob.getBytes(1, (int)blob.length());
        } else if(result instanceof Clob && destination.getName().equals("java.lang.String")) {
            try {
                Reader in = new BufferedReader(((Clob)result).getCharacterStream());
                char[] buf = new char[512];
                StringBuffer string = new StringBuffer("");
                int count;
                while((count = in.read(buf)) > -1)
                    string.append(buf, 0, count);
                in.close();
                return string.toString();
            } catch(IOException e) {
                log.error("Unable to read a CLOB column", e);
                throw new SQLException("Unable to read a CLOB column: " +e);
            }
        } else {
            bytes = rs.getBytes(idx);
        }

        if( bytes == null ) {
            result = null;
        } else if (destination.getName().equals("[B")) {
           return bytes;
        } else {
           // We should really reuse these guys

            ByteArrayInputStream bais = new ByteArrayInputStream(bytes);

           // Use the class loader to deserialize

            try {
            ObjectInputStream ois = new ObjectInputStream(bais);

            result = ((MarshalledObject) ois.readObject()).get();

            // ejb-reference: get the object back from the handle
            if (result instanceof Handle) result = ((Handle)result).getEJBObject();

            // is this a marshalled object that we stuck in earlier?
            if (result instanceof MarshalledObject && !destination.equals(MarshalledObject.class)) 
                result = ((MarshalledObject)result).get();
            
             if(!destination.isAssignableFrom(result.getClass())) {
                 boolean found = false;
                 if(destination.isPrimitive()) {
                     if((destination.equals(Byte.TYPE) && result instanceof Byte) ||
                        (destination.equals(Short.TYPE) && result instanceof Short) ||
                        (destination.equals(Character.TYPE) && result instanceof Character) ||
                        (destination.equals(Boolean.TYPE) && result instanceof Boolean) ||
                        (destination.equals(Integer.TYPE) && result instanceof Integer) ||
                        (destination.equals(Long.TYPE) && result instanceof Long) ||
                        (destination.equals(Float.TYPE) && result instanceof Float) ||
                        (destination.equals(Double.TYPE) && result instanceof Double)
                       ) {
                         found = true;
                     }
                 }
                 if(!found) {
                     if (log.isDebugEnabled())
                        log.debug("Unable to load a ResultSet column into a variable of type '"+destination.getName()+"' (got a "+result.getClass().getName()+")");
                     result = null;
                 }
             }

             ois.close();
         } catch (RemoteException e) {
            throw new SQLException("Unable to load EJBObject back from Handle: " +e);
            } catch (IOException e) {
                throw new SQLException("Unable to load a ResultSet column "+idx+" into a variable of type '"+destination.getName()+"': "+e);
            } catch (ClassNotFoundException e) {
                throw new SQLException("Unable to load a ResultSet column "+idx+" into a variable of type '"+destination.getName()+"': "+e);
            }
        }

        return result;
    }

   /**
    * Wrapper around getResultObject(ResultSet rs, int idx, Class destination).
    */
   protected Object getResultObject(ResultSet rs, int idx, CMPFieldMetaData cmpField)
      throws SQLException {
      if (!cmpField.isNested()) {
         // do it as before
         return getResultObject(rs, idx, cmpField.getField().getType());
      }
      
      // Assuming no one will ever use BLOPS in composite objects.
      // TODO Should be tested for BLOPability
      return rs.getObject(idx);
   }


   /**
    * Gets the integer JDBC type code corresponding to the given name.
    *
    * @param name the JDBC type name.
    * @return the JDBC type code.
    * @see Types
    */
   protected final int getJDBCType(String name)
   {
      try
      {
         Integer constant = (Integer)Types.class.getField(name).get(null);
         return constant.intValue();
      } catch (Exception e)
      {
         // JF: Dubious - better to throw a meaningful exception
         log.debug("Exception",e);
         return Types.OTHER;
      }
   }

   /**
    * Gets the JDBC type name corresponding to the given type code.
    *
    * @param jdbcType the integer JDBC type code.
    * @return the JDBC type name.
    * @see Types
    */
   protected final String getJDBCTypeName(int jdbcType)
   {
      if (jdbcTypeNames == null)
      {
         setUpJDBCTypeNames();
      }

      return (String)jdbcTypeNames.get(new Integer(jdbcType));
   }

   /**
    * Returns true if the JDBC type should be (de-)serialized as a
    * binary stream and false otherwise.
    *
    * @param jdbcType the JDBC type
    * @return true if binary type, false otherwise
    */
   protected final boolean isBinaryType(int jdbcType) {
       return (Types.BINARY == jdbcType ||
               Types.BLOB == jdbcType ||
               Types.CLOB == jdbcType ||
               Types.JAVA_OBJECT == jdbcType ||
               Types.LONGVARBINARY == jdbcType ||
               Types.OTHER == jdbcType ||
               Types.STRUCT == jdbcType ||
               Types.VARBINARY == jdbcType);
   }

   /**
    * Returns the comma-delimited list of primary key column names
    * for this entity.
    *
    * @return comma-delimited list of primary key column names.
    */
   protected final String getPkColumnList()
   {
      StringBuffer sb = new StringBuffer();
      Iterator it = jawsEntity.getPkFields();
      while (it.hasNext())
      {
         PkFieldMetaData pkFieldMetaData = (PkFieldMetaData)it.next();
         sb.append(pkFieldMetaData.getColumnName());
         if (it.hasNext())
         {
            sb.append(",");
         }
      }
      return sb.toString();
   }

   /**
    * Returns the string to go in a WHERE clause based on
    * the entity's primary key.
    *
    * @return WHERE clause content, in the form
    *  <code>pkCol1Name=? AND pkCol2Name=?</code>
    */
   protected final String getPkColumnWhereList()
   {
      StringBuffer sb = new StringBuffer();
      Iterator it = jawsEntity.getPkFields();
      while (it.hasNext())
      {
         PkFieldMetaData pkFieldMetaData = (PkFieldMetaData)it.next();
         sb.append(pkFieldMetaData.getColumnName());
         sb.append("=?");
         if (it.hasNext())
         {
            sb.append(" AND ");
         }
      }
      return sb.toString();
   }

   // MF: PERF!!!!!!!
   protected Object[] getState(EntityEnterpriseContext ctx)
   {
      Object[] state = new Object[jawsEntity.getNumberOfCMPFields()];
      Iterator iter = jawsEntity.getCMPFields();
      int i = 0;
      while (iter.hasNext())
      {
         CMPFieldMetaData fieldMetaData = (CMPFieldMetaData)iter.next();
         try
         {
            // JF: Should clone
            state[i++] = getCMPFieldValue(ctx.getInstance(), fieldMetaData);
         } catch (Exception e)
         {
            return null;
         }
      }

      return state;
   }

   protected Object getCMPFieldValue(Object instance, CMPFieldMetaData fieldMetaData)
      throws IllegalAccessException
   {
       return fieldMetaData.getValue(instance);
   }

   protected void setCMPFieldValue(Object instance,
                                   CMPFieldMetaData fieldMetaData,
                                   Object value)
      throws IllegalAccessException
   {
       if (fieldMetaData.isNested()) {
          // we have a nested field
          fieldMetaData.set(instance, value);
       }
       else {
          // the usual way
          Field field = fieldMetaData.getField();
          field.set(instance, value);
       }
   }

   protected Object getPkFieldValue(Object pk, PkFieldMetaData pkFieldMetaData)
      throws IllegalAccessException
   {
      Field field = pkFieldMetaData.getPkField();
      return field.get(pk);
   }

   // This is now only used in setForeignKey
   protected int getJawsCMPFieldJDBCType(CMPFieldMetaData fieldMetaData)
   {
      return fieldMetaData.getJDBCType();
   }

   // Private -------------------------------------------------------

   /** Get a database connection */
   protected Connection getConnection() throws SQLException
   {
      DataSource ds = jawsEntity.getDataSource();
      if (ds != null)
      {
        return ds.getConnection();
      } else throw new RuntimeException("Unable to locate data source!");
   }

   private final void setUpJDBCTypeNames()
   {
      jdbcTypeNames = new HashMap();

      Field[] fields = Types.class.getFields();
      int length = fields.length;
      for (int i = 0; i < length; i++) {
         Field f = fields[i];
         String fieldName = f.getName();
         try {
            Object fieldValue = f.get(null);
            jdbcTypeNames.put(fieldValue, fieldName);
         } catch (IllegalAccessException e) {
            // Should never happen
            log.error("IllegalAccessException",e);
         }
      }
   }

    class WorkaroundInputStream extends ObjectInputStream {
        public WorkaroundInputStream(java.io.InputStream source) throws IOException, java.io.StreamCorruptedException{
            super(source);
        }
        protected Class resolveClass(java.io.ObjectStreamClass v) throws IOException, ClassNotFoundException {
            try {
                return Class.forName(v.getName(), false, Thread.currentThread().getContextClassLoader());
            } catch(Exception e) {}
            return super.resolveClass(v);
        }
    }
}
