/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.jaws.metadata;

import java.util.Hashtable;
import java.util.Iterator;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.XmlLoadable;


/**
 *  <description>
 *
 *  @see <related>
 *  @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *  @version $Revision: 1.6 $
 */
public class TypeMappingMetaData extends MetaData implements XmlLoadable {
    // Constants -----------------------------------------------------
    
    public static final String[] PRIMITIVES = {"boolean","byte","char","short","int","long","float","double"};
    
    public static final String[] PRIMITIVE_CLASSES = {"java.lang.Boolean","java.lang.Byte","java.lang.Character","java.lang.Short","java.lang.Integer","java.lang.Long","java.lang.Float","java.lang.Double"};

    
    // Attributes ----------------------------------------------------

    private String name;
    
    private Hashtable mappings = new Hashtable();
    
    
    // Static --------------------------------------------------------

    // Constructors --------------------------------------------------

    // Public --------------------------------------------------------
    
    public String getName() { return name; }

    public String getSqlTypeForJavaType(Class type) {
        String javaType = type.getName();
    
        // Check primitive first
        for (int i = 0; i < PRIMITIVES.length; i++) {
            if (javaType.equals(PRIMITIVES[i])) {
                // Translate into class
                javaType = PRIMITIVE_CLASSES[i];
                break;
            }
        }
    
        // Check other types
        MappingMetaData mapping = (MappingMetaData)mappings.get(javaType);
        
        // if not found, return mapping for java.lang.object
        if (mapping == null) mapping = (MappingMetaData)mappings.get("java.lang.Object");
        
        return mapping.getSqlType();
    }



    public int getJdbcTypeForJavaType(Class type) {
        String javaType = type.getName();

        // Check primitive first
        for (int i = 0; i < PRIMITIVES.length; i++) {
            if (javaType.equals(PRIMITIVES[i])) {
                // Translate into class
                javaType = PRIMITIVE_CLASSES[i];
                break;
            }
        }

        // Check other types
        MappingMetaData mapping = (MappingMetaData)mappings.get(javaType);
        
        // if not found, return mapping for java.lang.object
        if (mapping == null) mapping = (MappingMetaData)mappings.get("java.lang.Object");
            
        return mapping.getJdbcType();
    }



    // XmlLoadable implementation ------------------------------------
    
    public void importXml(Element element) throws DeploymentException {
    
        // get the name of this type-mapping
        name = getElementContent(getUniqueChild(element, "name"));
        
        // get the mappings
        Iterator iterator = getChildrenByTagName(element, "mapping");
        
        while (iterator.hasNext()) {
            Element mappingElement = (Element)iterator.next();
            MappingMetaData mapping = new MappingMetaData();
            mapping.importXml(mappingElement);
            
            mappings.put(mapping.getJavaType(), mapping);
        }
    }
        
    // Package protected ---------------------------------------------

    // Protected -----------------------------------------------------

    // Private -------------------------------------------------------

    // Inner classes -------------------------------------------------
}
