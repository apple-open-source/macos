/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import java.util.HashMap;
import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

/**
 *   <description> 
 *      
 *   @see <related>
 *   @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 */
public class EjbLocalRefMetaData extends MetaData {
    // Constants -----------------------------------------------------
    
    // Attributes ----------------------------------------------------
	
   // the name used in the bean code
   private String name;
	
   // entity or session
   private String type;
	
	// the 2 interfaces
   private String localHome;
   private String local;
	
	// internal link: map name to link
   private String link;

   // external link: map name to jndiName
   private String jndiName;
	
   private HashMap invokerMap = new HashMap();

    // Static --------------------------------------------------------
    
    // Constructors --------------------------------------------------
	
    // Public --------------------------------------------------------
	
   public String getName() { return name; }
	
   public String getType() { return type; }
	
   public String getLocalHome() { return localHome; }
	
   public String getLocal() { return local; }
	
   public String getLink() { return link; }

   public String getJndiName() { return jndiName; }

   public String getInvokerBinding(String bindingName) { return (String)invokerMap.get(bindingName); }

    public void importEjbJarXml(Element element) throws DeploymentException {
      name = getElementContent(getUniqueChild(element, "ejb-ref-name"));
      type = getElementContent(getUniqueChild(element, "ejb-ref-type"));
      localHome = getElementContent(getUniqueChild(element, "local-home"));
      local = getElementContent(getUniqueChild(element, "local"));
      link = getElementContent(getOptionalChild(element, "ejb-link"));
   }
	
   public void importJbossXml(Element element) throws DeploymentException {
      jndiName = getElementContent(getOptionalChild(element, "local-jndi-name"));
   }
	
    public void importJbossXml(String invokerBinding, Element element) throws DeploymentException 
    {
      String refJndiName = getElementContent(getOptionalChild(element, "local-jndi-name"));
      invokerMap.put(invokerBinding, refJndiName);
    }   
}
