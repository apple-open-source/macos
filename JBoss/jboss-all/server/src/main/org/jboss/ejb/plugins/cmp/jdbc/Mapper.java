/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;


/**
 * Generally, implementations of this interface map instances of one Java type
 * into instances of another Java type.
 * Mappers are used in cases when instances of "enum" types are used as CMP
 * field values. In this case, a mapper represents a mediator and translates
 * instances of "enum" to some id when that can be stored in a column when storing
 * data and back from id to "enum" instance when data is loaded.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public interface Mapper
{
   /**
    * This method is called when CMP field is stored.
    * @param fieldValue - CMP field value
    * @return column value.
    */
   Object toColumnValue(Object fieldValue);

   /**
    * This method is called when CMP field is loaded.
    * @param columnValue - loaded column value.
    * @return CMP field value.
    */
   Object toFieldValue(Object columnValue);
}
