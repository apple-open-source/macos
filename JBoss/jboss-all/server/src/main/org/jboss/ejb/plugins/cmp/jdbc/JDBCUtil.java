/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Reader;
import java.io.StringReader;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.rmi.MarshalledObject;
import java.rmi.RemoteException;

import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.PreparedStatement;
import java.sql.Statement;
import java.sql.SQLException;
import java.sql.Types;
import java.sql.CallableStatement;

import java.util.Map;
import java.util.HashMap;
import java.math.BigDecimal;

import javax.ejb.EJBObject;
import javax.ejb.Handle;

import org.jboss.invocation.MarshalledValue;
import org.jboss.logging.Logger;

/**
 * JDBCUtil takes care of some of the more anoying JDBC tasks.
 * It hanles safe closing of jdbc resources, setting statement
 * parameters and loading query results.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @author Steve Coy
 */
public final class JDBCUtil
{
   private static final Logger log = Logger.getLogger(JDBCUtil.class.getName());

   public static void safeClose(Connection con)
   {
      if(con != null)
      {
         try
         {
            con.close();
         }
         catch(SQLException e)
         {
            log.error(SQL_ERROR, e);
         }
      }
   }

   public static void safeClose(ResultSet rs)
   {
      if(rs != null)
      {
         try
         {
            rs.close();
         }
         catch(SQLException e)
         {
            log.error(SQL_ERROR, e);
         }
      }
   }

   public static void safeClose(Statement statement)
   {
      if(statement != null)
      {
         try
         {
            statement.close();
         }
         catch(SQLException e)
         {
            log.error(SQL_ERROR, e);
         }
      }
   }

   private static void safeClose(InputStream in)
   {
      if(in != null)
      {
         try
         {
            in.close();
         }
         catch(IOException e)
         {
            log.error(SQL_ERROR, e);
         }
      }
   }

   private static void safeClose(OutputStream out)
   {
      if(out != null)
      {
         try
         {
            out.close();
         }
         catch(IOException e)
         {
            log.error(SQL_ERROR, e);
         }
      }
   }

   private static void safeClose(Reader reader)
   {
      if(reader != null)
      {
         try
         {
            reader.close();
         }
         catch(IOException e)
         {
            log.error(SQL_ERROR, e);
         }
      }
   }

   /**
    * Coerces the input value into the correct type for the specified
    * jdbcType.
    *
    * @param jdbcType the jdbc type to which the value will be assigned
    * @param value the value to coerce
    * @return the corrected object
    */
   private static Object coerceToSQLType(int jdbcType, Object value)
   {
      if(value.getClass() == java.util.Date.class)
      {
         if(jdbcType == Types.DATE)
         {
            return new java.sql.Date(((java.util.Date)value).getTime());
         }
         else if(jdbcType == Types.TIME)
         {
            return new java.sql.Time(((java.util.Date)value).getTime());
         }
         else if(jdbcType == Types.TIMESTAMP)
         {
            return new java.sql.Timestamp(((java.util.Date)value).getTime());
         }
      }
      else if(value.getClass() == Character.class && jdbcType == Types.VARCHAR)
      {
         value = value.toString();
      }
      return value;
   }

   /**
    * Coverts the value into a byte array.
    * @param value the value to convert into a byte array
    * @return the byte representation of the value
    * @throws SQLException if a problem occures in the conversion
    */
   private static byte[] convertObjectToByteArray(Object value)
      throws SQLException
   {
      // Do we already have a byte array?
      if(value instanceof byte[])
      {
         return (byte[])value;
      }

      ByteArrayOutputStream baos = null;
      ObjectOutputStream oos = null;
      try
      {
         // ejb-reference: store the handle
         if(value instanceof EJBObject)
         {
            value = ((EJBObject)value).getHandle();
         }

         // Marshall the object using MashalledValue to handle classloaders
         value = new MarshalledValue(value);

         // return the serialize the value
         baos = new ByteArrayOutputStream();
         oos = new ObjectOutputStream(baos);
         oos.writeObject(value);
         return baos.toByteArray();
      }
      catch(RemoteException e)
      {
         throw new SQLException("Cannot get Handle of EJBObject: " + e);
      }
      catch(IOException e)
      {
         throw new SQLException("Can't serialize binary object: " + e);
      }
      finally
      {
         safeClose(oos);
         safeClose(baos);
      }
   }

   /**
    * Coverts the input into an object.
    * @param input the bytes to convert
    * @return the object repsentation of the input stream
    * @throws SQLException if a problem occures in the conversion
    */
   private static Object convertToObject(byte[] input)
      throws SQLException
   {
      ByteArrayInputStream bais = new ByteArrayInputStream(input);
      try
      {
         return convertToObject(bais);
      }
      finally
      {
         safeClose(bais);
      }
   }


   /**
    * Coverts the input into an object.
    * @param input the bytes to convert
    * @return the object repsentation of the input stream
    * @throws SQLException if a problem occures in the conversion
    */
   private static Object convertToObject(InputStream input)
      throws SQLException
   {
      Object value = null;
      if(input != null)
      {
         ObjectInputStream ois = null;
         try
         {
            // deserialize result
            ois = new ObjectInputStream(input);
            value = ois.readObject();

            // de-marshall value if possible
            if(value instanceof MarshalledValue)
            {
               value = ((MarshalledValue)value).get();
            }
            else if(value instanceof MarshalledObject)
            {
               value = ((MarshalledObject)value).get();
            }

            // ejb-reference: get the object back from the handle
            if(value instanceof Handle)
            {
               value = ((Handle)value).getEJBObject();
            }

         }
         catch(RemoteException e)
         {
            throw new SQLException("Unable to load EJBObject back from Handle: " + e);
         }
         catch(IOException e)
         {
            throw new SQLException("Unable to load to deserialize result: " + e);
         }
         catch(ClassNotFoundException e)
         {
            throw new SQLException("Unable to load to deserialize result: " + e);
         }
         finally
         {
            safeClose(ois);
         }
      }
      return value;
   }

   /**
    * Get the indicated result set parameter as a character stream and return
    * it's entire content as a String.
    *
    * @param rs the <code>ResultSet</code> from which a result is
    *    being retrieved.
    * @param index index of the result column.
    * @return a String containing the content of the result column
    */
   private static String getLongString(ResultSet rs, int index)
      throws SQLException
   {
      String value;
      Reader textData = rs.getCharacterStream(index);
      if(textData != null)
      {
         try
         {
            // Use a modest buffer here to reduce function call overhead
            // when reading extremely large data.
            StringBuffer textBuffer = new StringBuffer();
            char[] tmpBuffer = new char[1000];
            int charsRead;
            while((charsRead = textData.read(tmpBuffer)) != -1)
               textBuffer.append(tmpBuffer, 0, charsRead);
            value = textBuffer.toString();
         }
         catch(java.io.IOException ioException)
         {
            throw new SQLException(ioException.getMessage());
         }
         finally
         {
            safeClose(textData);
         }
      }
      else
         value = null;
      return value;
   }


   /**
    * Read the entire input stream provided and return its content as a byte
    * array.
    *
    * @param input the <code>InputStream</code> from which a result is
    *    being retrieved.
    * @return a byte array containing the content of the input stream
    */
   private static byte[] getByteArray(InputStream input)
      throws SQLException
   {
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      try
      {
         // Use a modest buffer here to reduce function call overhead
         // when reading extremely large data.
         byte[] tmpBuffer = new byte[1000];
         int bytesRead;
         while((bytesRead = input.read(tmpBuffer)) != -1)
            baos.write(tmpBuffer, 0, bytesRead);
         return baos.toByteArray();
      }
      catch(java.io.IOException ioException)
      {
         throw new SQLException(ioException.getMessage());
      }
      finally
      {
         safeClose(baos);
      }
   }

   // Inner

   public static interface ResultSetReader
   {
      Object getFirst(ResultSet rs, Class destination) throws SQLException;

      Object get(ResultSet rs, int index, Class destination) throws SQLException;
   }

   private static abstract class AbstractResultSetReader
      implements ResultSetReader
   {
      public Object getFirst(ResultSet rs, Class destination)
         throws SQLException
      {
         return get(rs, 1, destination);
      }

      public Object get(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         Object result = readResult(rs, index, destination);
         if(result != null)
            result = coerceToJavaType(result, destination);
         return result;
      }

      protected abstract Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException;

      protected Object coerceToJavaType(Object value, Class destination)
         throws SQLException
      {
         try
         {
            //
            // java.rmi.MarshalledObject
            //
            // get unmarshalled value
            if(value instanceof MarshalledObject && !destination.equals(MarshalledObject.class))
            {
               value = ((MarshalledObject)value).get();
            }

            //
            // javax.ejb.Handle
            //
            // get the object back from the handle
            if(value instanceof Handle)
            {
               value = ((Handle)value).getEJBObject();
            }

            // Did we get the desired result?
            if(destination.isAssignableFrom(value.getClass()))
            {
               return value;
            }

            if(destination == java.math.BigInteger.class && value.getClass() == java.math.BigDecimal.class)
            {
               return ((java.math.BigDecimal)value).toBigInteger();
            }

            // oops got the wrong type - nothing we can do
            throw new SQLException("Got a " + value.getClass().getName() + "[cl=" +
               System.identityHashCode(value.getClass().getClassLoader()) +
               ", value=" + value + "] while looking for a " +
               destination.getName() + "[cl=" +
               System.identityHashCode(destination) + "]");
         }
         catch(RemoteException e)
         {
            throw new SQLException("Unable to load EJBObject back from Handle: " + e);
         }
         catch(IOException e)
         {
            throw new SQLException("Unable to load to deserialize result: " + e);
         }
         catch(ClassNotFoundException e)
         {
            throw new SQLException("Unable to load to deserialize result: " + e);
         }
      }
   }

   public static final ResultSetReader CLOB_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = JDBCUtil.getLongString(rs, index);
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Big Char, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader LONGVARCHAR_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = JDBCUtil.getLongString(rs, index);
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Big Char, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader BINARY_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = null;
         ;
         byte[] bytes = rs.getBytes(index);
         if(!rs.wasNull())
         {
            if(destination == byte[].class)
               value = bytes;
            else
               value = convertToObject(bytes);
         }
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Binary, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader VARBINARY_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = null;
         byte[] bytes = rs.getBytes(index);
         if(!rs.wasNull())
         {
            if(destination == byte[].class)
               value = bytes;
            else
               value = convertToObject(bytes);
         }
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Binary, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader BLOB_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = null;
         InputStream binaryData = rs.getBinaryStream(index);
         if(binaryData != null)
         {
            try
            {
               if(destination == byte[].class)
                  value = getByteArray(binaryData);
               else
                  value = convertToObject(binaryData);
            }
            finally
            {
               safeClose(binaryData);
            }
         }
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Big Binary, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader LONGVARBINARY_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = null;
         InputStream binaryData = rs.getBinaryStream(index);
         if(binaryData != null)
         {
            try
            {
               if(destination == byte[].class)
                  value = getByteArray(binaryData);
               else
                  value = convertToObject(binaryData);
            }
            finally
            {
               safeClose(binaryData);
            }
         }
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Big Binary, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader JAVA_OBJECT_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = rs.getObject(index);
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Object, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader STRUCT_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = rs.getObject(index);
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Object, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader ARRAY_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = rs.getObject(index);
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Object, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader OTHER_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = rs.getObject(index);
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Object, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader JAVA_UTIL_DATE_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         return rs.getTimestamp(index);
      }

      protected Object coerceToJavaType(Object value, Class destination)
      {
         // make new copy as sub types have problems in comparions
         java.util.Date result;
         // handle timestamp special becauses it hoses the milisecond values
         if(value instanceof java.sql.Timestamp)
         {
            java.sql.Timestamp ts = (java.sql.Timestamp)value;
            // Timestamp returns whole seconds from getTime and partial
            // seconds are retrieved from getNanos()
            // Adrian Brock: Not in 1.4 it doesn't
            long temp = ts.getTime();
            if(temp % 1000 == 0)
               temp += ts.getNanos() / 1000000;
            result = new java.util.Date(temp);
         }
         else
         {
            result = new java.util.Date(((java.util.Date)value).getTime());
         }
         return result;
      }
   };

   public static final ResultSetReader JAVA_SQL_DATE_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         return rs.getDate(index);
      }

      protected Object coerceToJavaType(Object value, Class destination)
      {
         // make a new copy object; you never know what a driver will return
         return new java.sql.Date(((java.sql.Date)value).getTime());
      }
   };

   public static final ResultSetReader JAVA_SQL_TIME_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         return rs.getTime(index);
      }

      protected Object coerceToJavaType(Object value, Class destination)
      {
         // make a new copy object; you never know what a driver will return
         return new java.sql.Time(((java.sql.Time)value).getTime());
      }
   };

   public static final ResultSetReader JAVA_SQL_TIMESTAMP_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         return rs.getTimestamp(index);
      }

      protected Object coerceToJavaType(Object value, Class destination)
      {
         // make a new copy object; you never know what a driver will return
         java.sql.Timestamp orignal = (java.sql.Timestamp)value;
         java.sql.Timestamp copy = new java.sql.Timestamp(orignal.getTime());
         copy.setNanos(orignal.getNanos());
         return copy;
      }
   };

   public static final ResultSetReader BIGDECIMAL_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return rs.getBigDecimal(index);
      }
   };

   public static final ResultSetReader REF_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return rs.getRef(index);
      }
   };

   public static final ResultSetReader BYTE_ARRAY_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return rs.getBytes(index);
      }
   };

   public static final ResultSetReader OBJECT_READER = new AbstractResultSetReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination) throws SQLException
      {
         Object value = rs.getObject(index);
         if(log.isTraceEnabled())
         {
            log.trace("Get result: index=" + index +
               ", javaType=" + destination.getName() +
               ", Object, value=" + value);
         }
         return value;
      }
   };

   public static final ResultSetReader STRING_READER = new ResultSetReader()
   {
      public Object getFirst(ResultSet rs, Class destination)
         throws SQLException
      {
         return rs.getString(1);
      }

      public Object get(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return rs.getString(index);
      }
   };

   private static abstract class AbstractPrimitiveReader
      extends AbstractResultSetReader
   {
      // ResultSetReader implementation

      public Object get(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         Object result = readResult(rs, index, destination);
         if(rs.wasNull())
            result = null;
         else
            result = coerceToJavaType(result, destination);
         return result;
      }

      // Protected
      protected Object coerceToJavaType(Object value, Class destination)
         throws SQLException
      {
         return value;
      }
   }

   public static final ResultSetReader BOOLEAN_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return (rs.getBoolean(index) ? Boolean.TRUE : Boolean.FALSE);
      }
   };

   public static final ResultSetReader BYTE_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return new Byte(rs.getByte(index));
      }
   };

   public static final ResultSetReader CHARACTER_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return rs.getString(index);
      }

      protected Object coerceToJavaType(Object value, Class destination)
      {
         //
         // java.lang.String --> java.lang.Character or char
         //
         // just grab first character
         if(value instanceof String && (destination == Character.class || destination == Character.TYPE))
         {
            return new Character(((String)value).charAt(0));
         }
         else
         {
            return value;
         }
      }
   };

   public static final ResultSetReader SHORT_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return new Short(rs.getShort(index));
      }
   };

   public static final ResultSetReader INT_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return new Integer(rs.getInt(index));
      }
   };

   public static final ResultSetReader LONG_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return new Long(rs.getLong(index));
      }
   };

   public static final ResultSetReader FLOAT_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return new Float(rs.getFloat(index));
      }
   };

   public static final ResultSetReader DOUBLE_READER = new AbstractPrimitiveReader()
   {
      protected Object readResult(ResultSet rs, int index, Class destination)
         throws SQLException
      {
         return new Double(rs.getDouble(index));
      }
   };

   public static ResultSetReader getResultSetReader(int jdbcType, Class destination)
   {
      ResultSetReader reader;
      switch(jdbcType)
      {
         case Types.CLOB:
            reader = CLOB_READER;
            break;
         case Types.LONGVARCHAR:
            reader = LONGVARCHAR_READER;
            break;
         case Types.BINARY:
            reader = BINARY_READER;
            break;
         case Types.VARBINARY:
            reader = VARBINARY_READER;
            break;
         case Types.BLOB:
            reader = BLOB_READER;
            break;
         case Types.LONGVARBINARY:
            reader = LONGVARBINARY_READER;
            break;
         case Types.JAVA_OBJECT:
            reader = JAVA_OBJECT_READER;
            break;
         case Types.STRUCT:
            reader = STRUCT_READER;
            break;
         case Types.ARRAY:
            reader = ARRAY_READER;
            break;
         case Types.OTHER:
            reader = OTHER_READER;
            break;
         default:
            {
               if(destination == java.util.Date.class)
               {
                  reader = JAVA_UTIL_DATE_READER;
               }
               else if(destination == java.sql.Date.class)
               {
                  reader = JAVA_SQL_DATE_READER;
               }
               else if(destination == java.sql.Time.class)
               {
                  reader = JAVA_SQL_TIME_READER;
               }
               else if(destination == java.sql.Timestamp.class)
               {
                  reader = JAVA_SQL_TIMESTAMP_READER;
               }
               else if(destination == java.math.BigDecimal.class)
               {
                  reader = BIGDECIMAL_READER;
               }
               else if(destination == java.sql.Ref.class)
               {
                  reader = REF_READER;
               }
               else if(destination == String.class)
               {
                  reader = STRING_READER;
               }
               else if(destination == Boolean.class || destination == Boolean.TYPE)
               {
                  reader = BOOLEAN_READER;
               }
               else if(destination == Byte.class || destination == Byte.TYPE)
               {
                  reader = BYTE_READER;
               }
               else if(destination == Character.class || destination == Character.TYPE)
               {
                  reader = CHARACTER_READER;
               }
               else if(destination == Short.class || destination == Short.TYPE)
               {
                  reader = SHORT_READER;
               }
               else if(destination == Integer.class || destination == Integer.TYPE)
               {
                  reader = INT_READER;
               }
               else if(destination == Long.class || destination == Long.TYPE)
               {
                  reader = LONG_READER;
               }
               else if(destination == Float.class || destination == Float.TYPE)
               {
                  reader = FLOAT_READER;
               }
               else if(destination == Double.class || destination == Double.TYPE)
               {
                  reader = DOUBLE_READER;
               }
               else
               {
                  reader = OBJECT_READER;
               }
            }
      }
      return reader;
   }


   //
   // All the above is used only for Oracle specific entity create command
   // and could be refactored/optimized.
   //

   private static final Map jdbcTypeNames;
   private final static Map csTypes;

   /**
    * Gets the JDBC type name corresponding to the given type code.
    * Only used in debug log messages.
    *
    * @param jdbcType the integer JDBC type code.
    * @return the JDBC type name.
    * @see Types
    */
   private static String getJDBCTypeName(int jdbcType)
   {
      return (String)jdbcTypeNames.get(new Integer(jdbcType));
   }

   private static final String SQL_ERROR = "SQL error";
   private static final String GET_TIMESTAMP = "getTimestamp";
   private static final String GET_DATE = "getDate";
   private static final String GET_TIME = "getTime";
   private static final String GET_BIGDECIMAL = "getBigDecimal";
   private static final String GET_REF = "getRef";
   private static final String GET_STRING = "getString";
   private static final String GET_BOOLEAN = "getBoolean";
   private static final String GET_BYTE = "getByte";
   private static final String GET_SHORT = "getShort";
   private static final String GET_INT = "getInt";
   private static final String GET_LONG = "getLong";
   private static final String GET_FLOAT = "getFloat";
   private static final String GET_DOUBLE = "getDouble";
   private static final String GET_BYTES = "getBytes";

   static
   {
      Class[] arg = new Class[]{Integer.TYPE};

      // Initialize the mapping between non-binary java result set
      // types and the method on CallableStatement that is used to retrieve
      // a value of the java type.
      csTypes = new HashMap();
      try
      {
         // java.util.Date
         csTypes.put(java.util.Date.class.getName(),
            CallableStatement.class.getMethod(GET_TIMESTAMP, arg));
         // java.sql.Date
         csTypes.put(java.sql.Date.class.getName(),
            CallableStatement.class.getMethod(GET_DATE, arg));
         // Time
         csTypes.put(java.sql.Time.class.getName(),
            CallableStatement.class.getMethod(GET_TIME, arg));
         // Timestamp
         csTypes.put(java.sql.Timestamp.class.getName(),
            CallableStatement.class.getMethod(GET_TIMESTAMP, arg));
         // BigDecimal
         csTypes.put(java.math.BigDecimal.class.getName(),
            CallableStatement.class.getMethod(GET_BIGDECIMAL, arg));
         // java.sql.Ref Does this really work?
         csTypes.put(java.sql.Ref.class.getName(),
            CallableStatement.class.getMethod(GET_REF, arg));
         // String
         csTypes.put(java.lang.String.class.getName(),
            CallableStatement.class.getMethod(GET_STRING, arg));
         // Boolean
         csTypes.put(java.lang.Boolean.class.getName(),
            CallableStatement.class.getMethod(GET_BOOLEAN, arg));
         // boolean
         csTypes.put(Boolean.TYPE.getName(),
            CallableStatement.class.getMethod(GET_BOOLEAN, arg));
         // Byte
         csTypes.put(java.lang.Byte.class.getName(),
            CallableStatement.class.getMethod(GET_BYTE, arg));
         // byte
         csTypes.put(Byte.TYPE.getName(),
            CallableStatement.class.getMethod(GET_BYTE, arg));
         // Character
         csTypes.put(java.lang.Character.class.getName(),
            CallableStatement.class.getMethod(GET_STRING, arg));
         // char
         csTypes.put(Character.TYPE.getName(),
            CallableStatement.class.getMethod(GET_STRING, arg));
         // Short
         csTypes.put(java.lang.Short.class.getName(),
            CallableStatement.class.getMethod(GET_SHORT, arg));
         // short
         csTypes.put(Short.TYPE.getName(),
            CallableStatement.class.getMethod(GET_SHORT, arg));
         // Integer
         csTypes.put(java.lang.Integer.class.getName(),
            CallableStatement.class.getMethod(GET_INT, arg));
         // int
         csTypes.put(Integer.TYPE.getName(),
            CallableStatement.class.getMethod(GET_INT, arg));
         // Long
         csTypes.put(java.lang.Long.class.getName(),
            CallableStatement.class.getMethod(GET_LONG, arg));
         // long
         csTypes.put(Long.TYPE.getName(),
            CallableStatement.class.getMethod(GET_LONG, arg));
         // Float
         csTypes.put(java.lang.Float.class.getName(),
            CallableStatement.class.getMethod(GET_FLOAT, arg));
         // float
         csTypes.put(Float.TYPE.getName(),
            CallableStatement.class.getMethod(GET_FLOAT, arg));
         // Double
         csTypes.put(java.lang.Double.class.getName(),
            CallableStatement.class.getMethod(GET_DOUBLE, arg));
         // double
         csTypes.put(Double.TYPE.getName(),
            CallableStatement.class.getMethod(GET_DOUBLE, arg));
         // byte[]   (scoy: I expect that this will no longer be invoked)
         csTypes.put("[B",
            CallableStatement.class.getMethod(GET_BYTES, arg));
      }
      catch(NoSuchMethodException e)
      {
         // Should never happen
         log.error(SQL_ERROR, e);
      }

      // Initializes the map between jdbcType (int) and the name of the type.
      // This map is used to print meaningful debug and error messages.
      jdbcTypeNames = new HashMap();
      Field[] fields = Types.class.getFields();
      for(int i = 0; i < fields.length; i++)
      {
         try
         {
            jdbcTypeNames.put(fields[i].get(null), fields[i].getName());
         }
         catch(IllegalAccessException e)
         {
            // Should never happen
            log.error(SQL_ERROR, e);
         }
      }
   }

   /**
    * Sets a parameter in this Command's PreparedStatement.
    * Handles null values, and provides tracing.
    *
    * @param ps the PreparedStatement whose parameter needs to be set.
    * @param index the index (1-based) of the parameter to be set.
    * @param jdbcType the JDBC type of the parameter.
    * @param value the value which the parameter is to be set to.
    * @throws SQLException if parameter setting fails.
    */
   public static void setParameter(
      Logger log,
      PreparedStatement ps,
      int index,
      int jdbcType,
      Object value) throws SQLException
   {
      if(log.isTraceEnabled())
      {
         log.trace("Set parameter: " +
            "index=" + index + ", " +
            "jdbcType=" + getJDBCTypeName(jdbcType) + ", " +
            "value=" + ((value == null) ? "NULL" : value));
      }

      //
      // null
      //
      if(value == null)
      {
         ps.setNull(index, jdbcType);
         return;
      }

      //
      // coerce parameter into correct SQL type
      // (for DATE, TIME, TIMESTAMP, CHAR)
      //
      value = coerceToSQLType(jdbcType, value);

      // Set the prepared statement parameter based upon the jdbc type
      switch(jdbcType)
      {
         //
         // Large character types
         //
         case Types.CLOB:
         case Types.LONGVARCHAR:
            {
               String string = value.toString();
               ps.setCharacterStream(index, new StringReader(string), string.length());
               // Can't close the reader because some drivers don't use it until
               // the statement is executed. This would appear to be a safe thing
               // to do with a StringReader as it only releases its reference.
            }
            break;

            //
            // All binary types
            //
            /*    case Types.JAVA_OBJECT:    // scoy: I'm not convinced that these should be here
                  case Types.OTHER:          // ie. mixed in with the binary types.
                  case Types.STRUCT:
             */
            //
            // Small binary types
            //
         case Types.BINARY:
         case Types.VARBINARY:
            {
               byte[] bytes = convertObjectToByteArray(value);
               ps.setBytes(index, bytes);
            }
            break;

            //
            // Large binary types
            //
         case Types.BLOB:
         case Types.LONGVARBINARY:
            {
               byte[] bytes = convertObjectToByteArray(value);
               ps.setBinaryStream(index, new ByteArrayInputStream(bytes), bytes.length);
               // Can't close the stream because some drivers don't use it until
               // the statement is executed. This would appear to be a safe thing
               // to do with a ByteArrayInputStream as it only releases its reference.
            }
            break;

            //
            // Some drivers e.g. Sybase jConnect 5.5 assume scale of 0 with setObject(index, value, type)
            // resulting in truncation of BigDecimals. However, setting scale to
            // zero for other datatypes may result in truncation for other inexact
            // numerics. Instead explictly use setBigDecimal when assigning
            // BigDecimals to a DECIMAL or NUMERIC column
         case Types.DECIMAL:
         case Types.NUMERIC:
            if(value instanceof BigDecimal)
            {
               ps.setBigDecimal(index, (BigDecimal)value);
            }
            else
            {
               ps.setObject(index, value, jdbcType, 0);
            }
            break;

            //
            // Let the JDBC driver handle these if it can.
            // If it can't, then don't use them!
            // Map to a binary type instead and let JBoss marshall/unmarshall the data.
            //
         case Types.JAVA_OBJECT:
         case Types.OTHER:
         case Types.STRUCT:

            //
            //  Standard SQL type
            //
         default:
            ps.setObject(index, value, jdbcType);
            break;

      }
   }

   /**
    * Used for all retrieval of parameters from <code>CallableStatement</code>s.
    * Implements tracing, and allows some tweaking of returned types.
    *
    * @param log where to log to
    * @param cs the <code>CallableStatement</code> from which an out parameter is being retrieved
    * @param index index of the result column.
    * @param jdbcType a {@link java.sql.Types} constant used to determine the
    *                 most appropriate way to extract the data from rs.
    * @param destination The class of the variable this is going into
    * @return the value
    */
   public static Object getParameter(Logger log, CallableStatement cs, int index, int jdbcType, Class destination) throws SQLException
   {
      Object value = null;
      switch(jdbcType)
      {
         //
         // Large types
         //
         case Types.CLOB:
         case Types.LONGVARCHAR:
         case Types.BLOB:
         case Types.LONGVARBINARY:
            throw new UnsupportedOperationException();

            //
            // Small binary types
            //
         case Types.BINARY:
         case Types.VARBINARY:
            {
               byte[] bytes = cs.getBytes(index);
               if(!cs.wasNull())
               {
                  if(destination == byte[].class)
                     value = bytes;
                  else
                     value = convertToObject(bytes);
               }
               if(log.isTraceEnabled())
               {
                  log.trace("Get result: index=" + index +
                     ", javaType=" + destination.getName() +
                     ", Binary, value=" + value);
               }
            }
            break;

            //
            // Specialist types that the
            // driver should handle
            //
         case Types.JAVA_OBJECT:
         case Types.STRUCT:
         case Types.ARRAY:
         case Types.OTHER:
            {
               value = cs.getObject(index);
               if(log.isTraceEnabled())
               {
                  log.trace("Get result: index=" + index +
                     ", javaType=" + destination.getName() +
                     ", Object, value=" + value);
               }
            }
            break;

            //
            // Non-binary types
            //
         default:
            Method method = (Method)csTypes.get(destination.getName());
            if(method != null)
            {
               try
               {
                  value = method.invoke(cs, new Object[]{new Integer(index)});
                  if(cs.wasNull())
                  {
                     value = null;
                  }

                  if(log.isTraceEnabled())
                  {
                     log.trace("Get result: index=" + index +
                        ", javaType=" + destination.getName() +
                        ", Simple, value=" + value);
                  }
               }
               catch(IllegalAccessException e)
               {
                  // Whatever, I guess non-binary will not work for this field.
               }
               catch(InvocationTargetException e)
               {
                  // Whatever, I guess non-binary will not work for this field.
               }
            }
            else
            {
               value = cs.getObject(index);
               if(log.isTraceEnabled())
               {
                  log.trace("Get result: index=" + index +
                     ", javaType=" + destination.getName() +
                     ", Object, value=" + value);
               }
            }
      }
      return coerceToJavaType(value, destination);
   }

   private static Object coerceToJavaType(
      Object value,
      Class destination) throws SQLException
   {
      try
      {
         //
         // null
         //
         if(value == null)
         {
            return null;
         }

         //
         // java.rmi.MarshalledObject
         //
         // get unmarshalled value
         if(value instanceof MarshalledObject && !destination.equals(MarshalledObject.class))
         {
            value = ((MarshalledObject)value).get();
         }

         //
         // javax.ejb.Handle
         //
         // get the object back from the handle
         if(value instanceof Handle)
         {
            value = ((Handle)value).getEJBObject();
         }

         //
         // Primitive wrapper classes
         //
         // We have a primitive wrapper and we want a real primitive
         // just return the wrapper and the vm will convert it at the proxy
         if(destination.isPrimitive())
         {
            if(value == null)
               throw new IllegalStateException("Loaded NULL value for a field of a primitive type.");
            if((destination.equals(Byte.TYPE) && value instanceof Byte) ||
               (destination.equals(Short.TYPE) && value instanceof Short) ||
               (destination.equals(Character.TYPE) && value instanceof Character) ||
               (destination.equals(Boolean.TYPE) && value instanceof Boolean) ||
               (destination.equals(Integer.TYPE) && value instanceof Integer) ||
               (destination.equals(Long.TYPE) && value instanceof Long) ||
               (destination.equals(Float.TYPE) && value instanceof Float) ||
               (destination.equals(Double.TYPE) && value instanceof Double)
            )
            {
               return value;
            }
         }

         //
         // java.util.Date
         //
         // make new copy as sub types have problems in comparions
         if(destination == java.util.Date.class && value instanceof java.util.Date)
         {
            // handle timestamp special becauses it hoses the milisecond values
            if(value instanceof java.sql.Timestamp)
            {
               java.sql.Timestamp ts = (java.sql.Timestamp)value;

               // Timestamp returns whole seconds from getTime and partial
               // seconds are retrieved from getNanos()
               // Adrian Brock: Not in 1.4 it doesn't
               long temp = ts.getTime();
               if(temp % 1000 == 0)
                  temp += ts.getNanos() / 1000000;
               return new java.util.Date(temp);
            }
            else
            {
               return new java.util.Date(((java.util.Date)value).getTime());
            }
         }

         //
         // java.sql.Time
         //
         // make a new copy object; you never know what a driver will return
         if(destination == java.sql.Time.class && value instanceof java.sql.Time)
         {
            return new java.sql.Time(((java.sql.Time)value).getTime());
         }

         //
         // java.sql.Date
         //
         // make a new copy object; you never know what a driver will return
         if(destination == java.sql.Date.class && value instanceof java.sql.Date)
         {
            return new java.sql.Date(((java.sql.Date)value).getTime());
         }

         //
         // java.sql.Timestamp
         //
         // make a new copy object; you never know what a driver will return
         if(destination == java.sql.Timestamp.class && value instanceof java.sql.Timestamp)
         {
            // make a new Timestamp object; you never know
            // what a driver will return
            java.sql.Timestamp orignal = (java.sql.Timestamp)value;
            java.sql.Timestamp copy = new java.sql.Timestamp(orignal.getTime());
            copy.setNanos(orignal.getNanos());
            return copy;
         }

         //
         // java.lang.String --> java.lang.Character or char
         //
         // just grab first character
         if(value instanceof String && (destination == Character.class || destination == Character.TYPE))
         {
            return new Character(((String)value).charAt(0));
         }

         // Did we get the desired result?
         if(destination.isAssignableFrom(value.getClass()))
         {
            return value;
         }

         if(destination == java.math.BigInteger.class && value.getClass() == java.math.BigDecimal.class)
         {
            return ((java.math.BigDecimal)value).toBigInteger();
         }

         // oops got the wrong type - nothing we can do
         throw new SQLException("Got a " + value.getClass().getName() + "[cl=" +
            System.identityHashCode(value.getClass().getClassLoader()) +
            ", value=" + value + "] while looking for a " +
            destination.getName() + "[cl=" +
            System.identityHashCode(destination) + "]");
      }
      catch(RemoteException e)
      {
         throw new SQLException("Unable to load EJBObject back from Handle: "
            + e);
      }
      catch(IOException e)
      {
         throw new SQLException("Unable to load to deserialize result: " + e);
      }
      catch(ClassNotFoundException e)
      {
         throw new SQLException("Unable to load to deserialize result: " + e);
      }
   }
}
