/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List; 
import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;

/**
 * Imutable class which holds a list of the properties for a dependent value
 * class.
 *     
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 *   @version $Revision: 1.5 $
 */
public final class JDBCValueClassMetaData {
   private final Class javaType;
   private final List properties;

   /**
    * Constructs a value class metadata class with the data contained in 
    * the dependent-value-class xml element from a jbosscmp-jdbc xml file.
    *
    * @param classElement the xml Element which contains the metadata about
    *       this value class
    * @param classLoader the ClassLoader which is used to load this value class 
    * @throws DeploymentException if the xml element is not semantically correct
    */
   public JDBCValueClassMetaData(Element classElement, ClassLoader classLoader) throws DeploymentException {      
      String className = MetaData.getUniqueChildContent(classElement, "class");
      try {
         javaType = classLoader.loadClass(className);
      } catch (ClassNotFoundException e) {
         throw new DeploymentException("dependent-value-class not found: " + className);
      }
      
      List propertyList = new ArrayList();
      Iterator iterator = MetaData.getChildrenByTagName(classElement, "property");
      while(iterator.hasNext()) {
         Element propertyElement = (Element)iterator.next();
      
         propertyList.add(new JDBCValuePropertyMetaData(propertyElement, javaType));
      }
      properties = Collections.unmodifiableList(propertyList);
   }

   /**
    * Gets the Java Class of this value class.
    *
    * @return the java Class of this value class
    */
   public Class getJavaType() {
      return javaType;
   }

   /**
    * Gets the properties of this value class which are to be saved into the database.
    *
    * @return an unmodifiable list which contains the JDBCValuePropertyMetaData objects
    */
   public List getProperties() {
      return properties;
   }
}
