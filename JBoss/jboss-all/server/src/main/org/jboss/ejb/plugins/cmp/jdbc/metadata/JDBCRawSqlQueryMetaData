/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.lang.reflect.Method;

/**
 * Imutable class which holds information about a raw sql query.
 * A raw sql query allows you to do anything sql allows you to do.
 *    
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 *   @version $Revision: 1.6 $
 */
public final class JDBCRawSqlQueryMetaData implements JDBCQueryMetaData {
   private final Method method;

   /**
    * Constructs a JDBCRawSqlQueryMetaData which is invoked by the specified
    * method.
    * @param method the method which invokes this query
    */
   public JDBCRawSqlQueryMetaData(Method method) {
      this.method = method;
   }

   public Method getMethod() {
      return method;
   }

   public boolean isResultTypeMappingLocal() {
      return false;
   }

   /**
    * Gets the read ahead metadata for the query.
    * @return the read ahead metadata for the query.
    */
   public JDBCReadAheadMetaData getReadAhead() {
      return JDBCReadAheadMetaData.DEFAULT;
   }



   /**
    * Compares this JDBCRawSqlQueryMetaData against the specified object. Returns
    * true if the objects are the same. Two JDBCRawSqlQueryMetaData are the same
    * if they are both invoked by the same method.
    * @param o the reference object with which to compare
    * @return true if this object is the same as the object argument; false otherwise
    */
   public boolean equals(Object o) {
      if(o instanceof JDBCRawSqlQueryMetaData) {
         return ((JDBCRawSqlQueryMetaData)o).method.equals(method);
      }
      return false;
   }
   
   /**
    * Returns a hashcode for this JDBCRawSqlQueryMetaData. The hashcode is computed
    * by the method which invokes this query.
    * @return a hash code value for this object
    */
   public int hashCode() {
      return method.hashCode();
   }
   /**
    * Returns a string describing this JDBCRawSqlQueryMetaData. The exact details
    * of the representation are unspecified and subject to change, but the following
    * may be regarded as typical:
    * 
    * "[JDBCRawSqlQueryMetaData: method=public org.foo.User findByName(java.lang.String)]"
    *
    * @return a string representation of the object
    */
   public String toString() {
      return "[JDBCRawSqlQueryMetaData : method=" + method + "]";
   }
}
