/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import javax.naming.Context;
import javax.naming.NamingException;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.naming.Util;

/**
 *   <description> 
 *      
 *   @see <related>
 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @version $Revision: 1.6 $
 */
public class EnvEntryMetaData extends MetaData {
    // Constants -----------------------------------------------------
    
    // Attributes ----------------------------------------------------
    private String name;
    private String type;
    private String value;
    // Static --------------------------------------------------------
    
    // Constructors --------------------------------------------------
    public EnvEntryMetaData () {
	}
	
    // Public --------------------------------------------------------
	
    public String getName() { return name; }

    public String getType() { return type; }

    public String getValue() { return value; }
    
    public void importEjbJarXml(Element element) throws DeploymentException {
        name = getElementContent(getUniqueChild(element, "env-entry-name"));
        type = getElementContent(getUniqueChild(element, "env-entry-type"));
        // Strip any surrounding spaces
        type = type.trim();
        value = getElementContent(getUniqueChild(element, "env-entry-value"));
    }
    
    public static void bindEnvEntry(Context ctx, EnvEntryMetaData entry)
        throws ClassNotFoundException, NamingException
    {
        ClassLoader loader = EnvEntryMetaData.class.getClassLoader();
        Class type = loader.loadClass(entry.getType());
        if( type == String.class )
        {
            Util.bind(ctx, entry.getName(), entry.getValue());
        }
        else if( type == Integer.class )
        {
            Util.bind(ctx, entry.getName(), new Integer(entry.getValue()));
        }
        else if( type == Long.class )
        {
            Util.bind(ctx, entry.getName(), new Long(entry.getValue()));
        }
        else if( type == Double.class )
        {
            Util.bind(ctx, entry.getName(), new Double(entry.getValue()));
        }
        else if( type == Float.class )
        {
            Util.bind(ctx, entry.getName(), new Float(entry.getValue()));
        }
        else if( type == Byte.class )
        {
            Util.bind(ctx, entry.getName(), new Byte(entry.getValue()));
        }
        else if( type == Short.class )
        {
            Util.bind(ctx, entry.getName(), new Short(entry.getValue()));
        }
        else if( type == Boolean.class )
        {
            Util.bind(ctx, entry.getName(), new Boolean(entry.getValue()));
        }
        else
        {
            // Default to a String type
            Util.bind(ctx, entry.getName(), entry.getValue());
        }
    }

}
