/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.jaws.metadata;

import java.sql.Types;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.XmlLoadable;

/**
 *  <description>
 *
 *  @see <related>
 *  @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *  @version $Revision: 1.6 $
 */
public class MappingMetaData extends MetaData implements XmlLoadable
{
    // Constants -----------------------------------------------------

    // Attributes ----------------------------------------------------
    protected static Logger log = Logger.getLogger(MappingMetaData.class);
    private String javaType;
    
    private int jdbcType;
    
    private String sqlType;
    
    
    // Static --------------------------------------------------------
    
    public static int getJdbcTypeFromName(String name) throws DeploymentException {     
        if (name == null) {
            throw new DeploymentException("jdbc-type cannot be null");
        }
        
        try {
            Integer constant = (Integer)Types.class.getField(name).get(null);
            return constant.intValue();
        
        } catch (Exception e) {
            log.warn("Unrecognized jdbc-type: " + name + ", using Types.OTHER", e);
            return Types.OTHER;
        }
    }
    
    
    // Constructors --------------------------------------------------

    // Public --------------------------------------------------------

    public String getJavaType() { return javaType; }
    
    public int getJdbcType() { return jdbcType; }
    
    public String getSqlType() { return sqlType; }
    
    
    // XmlLoadable implementation ------------------------------------
    
    public void importXml(Element element) throws DeploymentException {
    
        javaType = getElementContent(getUniqueChild(element, "java-type"));
        
        jdbcType = getJdbcTypeFromName(getElementContent(getUniqueChild(element, "jdbc-type")));
        
        sqlType = getElementContent(getUniqueChild(element, "sql-type"));
    }   
    
    // Package protected ---------------------------------------------

    // Protected -----------------------------------------------------

    // Private -------------------------------------------------------

    // Inner classes -------------------------------------------------
}
