/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.lang.reflect.Method;
import org.w3c.dom.Element;
import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.QueryMetaData;

/**
 * Immutable class which contains information about an JBossQL query.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.1 $
 */
public final class JDBCJBossQLQueryMetaData implements JDBCQueryMetaData {
   /**
    * The method to which this query is bound.
    */
   private final Method method;

   /**
    * The ejb-ql fro the query.
    */
   private final String jbossQL;

   /**
    * Should the query return Local or Remote beans.
    */
   private final boolean resultTypeMappingLocal;

   /**
    * Read ahead meta data.
    */
   private final JDBCReadAheadMetaData readAhead;

   /**
    * Constructs a JDBCJBossQLQueryMetaData with JBossQL declared in the 
    * jboss-ql elemnt and is invoked by the specified method.
    * @param queryMetaData the metadata about this query
    */
   public JDBCJBossQLQueryMetaData(
         JDBCJBossQLQueryMetaData defaults,
         JDBCReadAheadMetaData readAhead) throws DeploymentException {
      
      this.method = defaults.getMethod();
      this.readAhead = readAhead;
      this.jbossQL = defaults.getJBossQL();
      this.resultTypeMappingLocal = defaults.isResultTypeMappingLocal();
   }

   /**
    * Constructs a JDBCJBossQLQueryMetaData with JBossQL declared in the 
    * jboss-ql elemnt and is invoked by the specified method.
    * @param queryMetaData the metadata about this query
    */
   public JDBCJBossQLQueryMetaData(
         JDBCQueryMetaData jdbcQueryMetaData,
         Element element,
         Method method,
         JDBCReadAheadMetaData readAhead) throws DeploymentException {
      
      this.method = method;
      this.readAhead = readAhead;
      jbossQL = MetaData.getElementContent(element);
      if(jbossQL==null || jbossQL.trim().length()==0) {
         throw new DeploymentException("jboss-ql element is empty");
      }
      resultTypeMappingLocal = jdbcQueryMetaData.isResultTypeMappingLocal();
   }

   // javadoc in parent class
   public Method getMethod() {
      return method;
   }

   /**
    * Gets the JBossQL query which will be invoked.
    * @return the ejb ql String for this query
    */
   public String getJBossQL() {
      return jbossQL;
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
    * Compares this JDBCJBossQLQueryMetaData against the specified object.
    * Returns true if the objects are the same. Two JDBCJBossQLQueryMetaData
    * are the same if they are both invoked by the same method.
    * @param o the reference object with which to compare
    * @return true if this object is the same as the object argument; 
    *    false otherwise
    */
   public boolean equals(Object o) {
      if(o instanceof JDBCJBossQLQueryMetaData) {
         return ((JDBCJBossQLQueryMetaData)o).method.equals(method);
      }
      return false;
   }

   /**
    * Returns a hashcode for this JDBCJBossQLQueryMetaData. The hashcode is
    * computed by the method which invokes this query.
    * @return a hash code value for this object
    */
   public int hashCode() {
      return method.hashCode();
   }
   /**
    * Returns a string describing this JDBCJBossQLQueryMetaData. The exact
    * details of the representation are unspecified and subject to change, but
    * the following may be regarded as typical:
    * 
    * "[JDBCJBossQLQueryMetaData: method=public org.foo.User
    *       findByName(java.lang.String)]"
    *
    * @return a string representation of the object
    */
   public String toString() {
      return "[JDBCJBossQLQueryMetaData : method=" + method + "]";
   }
}
