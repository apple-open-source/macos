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


/**
 * Imutable class contains information about a declated query.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 *   @version $Revision: 1.11.4.2 $
 */
public final class JDBCDeclaredQueryMetaData implements JDBCQueryMetaData {
   /**
    * The method to which this query is bound.
    */
   private final Method method;

   /**
    * The user specified additional columns to be added to the select clause.
    */
   private final String additionalColumns;

   /**
    * The user specified from clause.
    */
   private final String from;

   /**
    * The user specified where clause.
    */
   private final String where;

   /**
    * The user specified order clause.
    */
   private final String order;

   /**
    * The other clause is appended to the end of the sql.  This is useful for
    * hints to the query engine.
    */
   private final String other;

   /**
    * Should the select be DISTINCT?
    */
   private final boolean distinct;
   
   /**
    * The name of the ejb from which the field will be selected.
    */
   private final String ejbName;

   /**
    * The name of the cmp-field to be selected.
    */
   private final String fieldName;

   /**
    * The aliase that is used for the main select table.
    */
   private final String alias;
   /**
    * Read ahead meta data.
    */
   private final JDBCReadAheadMetaData readAhead;

   /**
    * Should the query return Local or Remote beans.
    */
   private final boolean resultTypeMappingLocal;

   /**
    * Constructs a JDBCDeclaredQueryMetaData which is defined by the 
    * declared-sql xml element and is invoked by the specified method.
    * Inherits unspecified values from the defaults.
    * @param defaults the default values to use
    * @param readAhead the read-ahead properties for this query
    */
   public JDBCDeclaredQueryMetaData(
         JDBCDeclaredQueryMetaData defaults,
         JDBCReadAheadMetaData readAhead) throws DeploymentException {

      this.method = defaults.getMethod();
      this.readAhead = readAhead;

      this.from = defaults.getFrom();
      this.where = defaults.getWhere();
      this.order = defaults.getOrder();
      this.other = defaults.getOther();

      this.resultTypeMappingLocal = defaults.isResultTypeMappingLocal();

      this.distinct = defaults.isSelectDistinct();
      this.ejbName = defaults.getEJBName();
      this.fieldName = defaults.getFieldName();
      this.alias = defaults.getAlias();
      this.additionalColumns = defaults.getAdditionalColumns();
   }


   /**
    * Constructs a JDBCDeclaredQueryMetaData which is defined by the 
    * declared-sql xml element and is invoked by the specified method.
    * @param jdbcQueryMetaData metadata about the query
    * @param queryElement the xml Element which contains the metadata about 
    *       this query
    * @param method the method which invokes this query
    * @param readAhead the read-ahead properties for this query
    */
   public JDBCDeclaredQueryMetaData(
         JDBCQueryMetaData jdbcQueryMetaData,
         Element queryElement,
         Method method,
         JDBCReadAheadMetaData readAhead) throws DeploymentException {

      this.method = method;
      this.readAhead = readAhead;

      from = nullIfEmpty(MetaData.getOptionalChildContent(queryElement, "from"));
      where = nullIfEmpty(MetaData.getOptionalChildContent(queryElement, "where"));
      order = nullIfEmpty(MetaData.getOptionalChildContent(queryElement, "order"));
      other = nullIfEmpty(MetaData.getOptionalChildContent(queryElement, "other"));

      resultTypeMappingLocal = jdbcQueryMetaData.isResultTypeMappingLocal();

      // load ejbSelect info
      Element selectElement = 
            MetaData.getOptionalChild(queryElement, "select");
         
      if(selectElement != null) {
         // should select use distinct?
         distinct = 
            (MetaData.getOptionalChild(selectElement, "distinct") != null);
         
         if(method.getName().startsWith("ejbSelect")) {
            ejbName = MetaData.getUniqueChildContent(selectElement, "ejb-name");
            fieldName = nullIfEmpty(MetaData.getOptionalChildContent(selectElement, "field-name"));
         } else {
            // the ejb-name and field-name elements are not allowed for finders
            if(MetaData.getOptionalChild(selectElement, "ejb-name") != null) {
               throw new DeploymentException(
                     "The ejb-name element of declared-sql select is only " +
                     "allowed for ejbSelect queries.");
            }
            if(MetaData.getOptionalChild(selectElement, "field-name") != null) {
               throw new DeploymentException(
                     "The field-name element of declared-sql select is only " +
                     "allowed for ejbSelect queries.");
            }
            ejbName = null;
            fieldName = null;
         }
         alias = nullIfEmpty(MetaData.getOptionalChildContent(selectElement, "alias"));
         additionalColumns = nullIfEmpty(MetaData.getOptionalChildContent(selectElement, "additional-columns"));
      } else {
         if(method.getName().startsWith("ejbSelect")) {
            throw new DeploymentException("The select element of " +
                  "declared-sql is required for ejbSelect queries.");
         } 
         distinct = false;
         ejbName = null;
         fieldName = null;
         alias = null;
         additionalColumns = null;
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
    * Gets the sql FROM clause of this query.
    * @return a String which contains the sql FROM clause
    */
   public String getFrom() {
      return from;
   }

   /**
    * Gets the sql WHERE clause of this query.
    * @return a String which contains the sql WHERE clause
    */
   public String getWhere() {
      return where;
   }

   /**
    * Gets the sql ORDER BY clause of this query.
    * @return a String which contains the sql ORDER BY clause
    */
   public String getOrder() {
      return order;
   }
   
   /**
    * Gets other sql code which is appended to the end of the query.
    * This is userful for supplying hints to the query engine.
    * @return a String which contains additional sql code which is 
    *         appended to the end of the query
    */
   public String getOther() {
      return other;
   }

   /**
    * Should the select be DISTINCT?
    * @return true if the select clause should contain distinct
    */
   public boolean isSelectDistinct() {
      return distinct;
   }

   /**
    * The name of the ejb from which the field will be selected.
    * @return the name of the ejb from which a field will be selected, or null
    * if returning a whole ejb
    */
   public String getEJBName() {
      return ejbName;
   }

   /**
    * The name of the cmp-field to be selected.
    * @return the name of the cmp-field to be selected or null if returning a 
    * whole ejb
    */
   public String getFieldName() {
      return fieldName;
   }
   
   /**
    * The alias that is used for the select table.
    * @return the alias that is used for the table from which the entity or
    * field is selected.
    */
   public String getAlias() {
      return alias;
   }

   /**
    * Additional columns that should be added to the select clause. For example,
    * columns that are used in an order by clause.
    * @return additional columns that should be added to the select clause
    */
   public String getAdditionalColumns()
   {
      return additionalColumns;
   }

   /**
    * Compares this JDBCDeclaredQueryMetaData against the specified object.
    * Returns true if the objects are the same. Two JDBCDeclaredQueryMetaData
    * are the same if they are both invoked by the same method.
    * @param o the reference object with which to compare
    * @return true if this object is the same as the object argument; false
    * otherwise
    */
   public boolean equals(Object o) {
      if(o instanceof JDBCDeclaredQueryMetaData) {
         return ((JDBCDeclaredQueryMetaData)o).method.equals(method);
      }
      return false;
   }
   
   /**
    * Returns a hashcode for this JDBCDeclaredQueryMetaData. The hashcode is
    * computed by the method which invokes this query.
    * @return a hash code value for this object
    */
   public int hashCode() {
      return method.hashCode();
   }
   /**
    * Returns a string describing this JDBCDeclaredQueryMetaData. The exact 
    * details of the representation are unspecified and subject to change, 
    * but the following may be regarded as typical:
    * 
    * "[JDBCDeclaredQueryMetaData: method=public org.foo.User findByName(
    *    java.lang.String)]"
    *
    * @return a string representation of the object
    */
   public String toString() {
      return "[JDBCDeclaredQueryMetaData : method=" + method + "]";
   }

   private static String nullIfEmpty(String s) {
      if (s != null && s.trim().length() == 0) {
         s = null;
      }
      return s;
   }
}
