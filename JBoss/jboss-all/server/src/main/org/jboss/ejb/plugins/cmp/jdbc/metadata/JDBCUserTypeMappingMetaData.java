/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;


/**
 * Immutable class instances of which represent user type mappings.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class JDBCUserTypeMappingMetaData
{
   /** Fully qualified Java type name being mapped */
   private final String javaType;
   /** Fully qualified Java type name <code>javaType</code> is mapped to */
   private final String mappedType;
   /** Fully qualified Java type name of Mapper implementation */
   private final String mapper;

   public JDBCUserTypeMappingMetaData(Element userMappingEl)
      throws DeploymentException
   {
      javaType = MetaData.getUniqueChildContent(userMappingEl, "java-type");
      mappedType = MetaData.getUniqueChildContent(userMappingEl, "mapped-type");
      mapper = MetaData.getUniqueChildContent(userMappingEl, "mapper");
   }

   public String getJavaType()
   {
      return javaType;
   }

   public String getMappedType()
   {
      return mappedType;
   }

   public String getMapper()
   {
      return mapper;
   }
}
