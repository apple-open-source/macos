/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.lang.reflect.Method;

/**
 *   This immutable class contains information about an automatically generated
 * query. This class is a place holder used to make an automaticlly generated
 * query look more like a user specified query.  This class only contains a
 * referance to the method used to invoke this query.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 *   @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @version $Revision: 1.4.4.1 $
 */
public final class JDBCAutomaticQueryMetaData implements JDBCQueryMetaData {
   /**
    * A referance to the method which invokes this query.
    */
   private final Method method;

   /**
    * Read ahead meta data.
    */
   private final JDBCReadAheadMetaData readAhead;

   /**
    * Constructs a JDBCAutomaticQueryMetaData which is invoked by the specified
    * method.
    * @param method the method which invokes this query
    */
   /*
   public JDBCAutomaticQueryMetaData(Method method, JDBCReadAheadMetaData readAhead) {
      this.method = method;
      this.readAhead = JDBCReadAheadMetaData.DEFAULT;
   }
*/
   /**
    * Constructs a JDBCAutomaticQueryMetaData which is invoked by the specified
    * method.
    * @param method the method which invokes this query
    * @readAhead Read ahead meta data.
    */
   public JDBCAutomaticQueryMetaData(
         Method method,
         JDBCReadAheadMetaData readAhead) {

      this.method = method;
      this.readAhead = readAhead;
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
      return readAhead;
   }

    /**
    * Compares this JDBCAutomaticQueryMetaData against the specified object. Returns
    * true if the objects are the same. Two JDBCAutomaticQueryMetaData are the same
    * if they are both invoked by the same method.
    * @param o the reference object with which to compare
    * @return true if this object is the same as the object argument; false otherwise
    */
   public boolean equals(Object o) {
      if(o instanceof JDBCAutomaticQueryMetaData) {
         return ((JDBCAutomaticQueryMetaData)o).method.equals(method);
      }
      return false;
   }

   /**
    * Returns a hashcode for this JDBCAutomaticQueryMetaData. The hashcode is computed
    * by the method which invokes this query.
    * @return a hash code value for this object
    */
   public int hashCode() {
      return method.hashCode();
   }
   /**
    * Returns a string describing this JDBCAutomaticQueryMetaData. The exact details
    * of the representation are unspecified and subject to change, but the following
    * may be regarded as typical:
    *
    * "[JDBCAutomaticQueryMetaData: method=public org.foo.User findByName(java.lang.String)]"
    *
    * @return a string representation of the object
    */
   public String toString() {
      return "[JDBCAutomaticQueryMetaData : method=" + method + "]";
   }
}
