/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.lang.reflect.Method;
import org.jboss.deployment.DeploymentException;

/**
 * Immutable class which contains information about an DynamicQL query.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.3 $
 */
public final class JDBCDynamicQLQueryMetaData implements JDBCQueryMetaData {
   /**
    * The method to which this query is bound.
    */
   private final Method method;

   /**
    * Should the query return Local or Remote beans.
    */
   private final boolean resultTypeMappingLocal;

   private final JDBCReadAheadMetaData readAhead;

   /**
    * Constructs a JDBCDynamicQLQueryMetaData with DynamicQL declared in the 
    * jboss-ql elemnt and is invoked by the specified method.
    * @param defaults the metadata about this query
    */
   public JDBCDynamicQLQueryMetaData(
         JDBCDynamicQLQueryMetaData defaults,
         JDBCReadAheadMetaData readAhead) throws DeploymentException {
      
      this.method = defaults.getMethod();
      this.readAhead = readAhead;
      this.resultTypeMappingLocal = defaults.isResultTypeMappingLocal();
   }


   /**
    * Constructs a JDBCDynamicQLQueryMetaData with DynamicQL declared in the 
    * jboss-ql elemnt and is invoked by the specified method.
    * @param jdbcQueryMetaData the metadata about this query
    */
   public JDBCDynamicQLQueryMetaData(JDBCQueryMetaData jdbcQueryMetaData,
                                     Method method,
                                     JDBCReadAheadMetaData readAhead)
      throws DeploymentException {
      
      this.method = method;
      this.readAhead = readAhead;
      resultTypeMappingLocal = jdbcQueryMetaData.isResultTypeMappingLocal();

      Class[] parameterTypes = method.getParameterTypes();
      if(parameterTypes.length != 2 ||
            !parameterTypes[0].equals(String.class) ||
            !parameterTypes[1].equals(Object[].class)) {
         throw new DeploymentException("Dynamic-ql method must have two " +
               "parameters of type String and Object[].");
      }
   }

   // javadoc in parent class
   public Method getMethod() {
      return method;
   }

   // javadoc in parent class
   public boolean isResultTypeMappingLocal() {
      return resultTypeMappingLocal;
   }

   /**
    * Gets the read ahead metadata for the query.
    * @return the read ahead metadata for the query.
    */
   public JDBCReadAheadMetaData getReadAhead() {
      return readAhead;
   }

   /**
    * Compares this JDBCDynamicQLQueryMetaData against the specified object.
    * Returns true if the objects are the same. Two JDBCDynamicQLQueryMetaData
    * are the same if they are both invoked by the same method.
    * @param o the reference object with which to compare
    * @return true if this object is the same as the object argument; 
    *    false otherwise
    */
   public boolean equals(Object o) {
      if(o instanceof JDBCDynamicQLQueryMetaData) {
         return ((JDBCDynamicQLQueryMetaData)o).method.equals(method);
      }
      return false;
   }

   /**
    * Returns a hashcode for this JDBCDynamicQLQueryMetaData. The hashcode is
    * computed by the method which invokes this query.
    * @return a hash code value for this object
    */
   public int hashCode() {
      return method.hashCode();
   }
   /**
    * Returns a string describing this JDBCDynamicQLQueryMetaData. The exact
    * details of the representation are unspecified and subject to change, but
    * the following may be regarded as typical:
    * 
    * "[JDBCDynamicQLQueryMetaData: method=public org.foo.User
    *       findByName(java.lang.String)]"
    *
    * @return a string representation of the object
    */
   public String toString() {
      return "[JDBCDynamicQLQueryMetaData : method=" + method + "]";
   }
}
