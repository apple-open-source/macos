/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

import java.util.HashMap;

/**
 *   <description> 
 *      
 *   @see <related>
 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @version $Revision: 1.6 $
 */
public class EjbRefMetaData extends MetaData {
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
	
   // the name used in the bean code
   private String name;
	
   // entity or session
   private String type;
	
   // the 2 interfaces
   private String home;
   private String remote;
	
   // internal link: map name to link
   private String link;
	
   // external link: map name to jndiName
   private String jndiName;

   private HashMap invokerMap = new HashMap();
	
   // Static --------------------------------------------------------
    
   // Constructors --------------------------------------------------
   public EjbRefMetaData () {
   }
	
   // Public --------------------------------------------------------
	
   public String getName() { return name; }
	
   public String getType() { return type; }
	
   public String getHome() { return home; }
	
   public String getRemote() { return remote; }
	
   public String getLink() { return link; }

   public String getJndiName() { return jndiName; }

   public String getInvokerBinding(String bindingName) { return (String)invokerMap.get(bindingName); }

   public void importEjbJarXml(Element element) throws DeploymentException {
      name = getElementContent(getUniqueChild(element, "ejb-ref-name"));
      type = getElementContent(getUniqueChild(element, "ejb-ref-type"));
      home = getElementContent(getUniqueChild(element, "home"));
      remote = getElementContent(getUniqueChild(element, "remote"));
      link = getElementContent(getOptionalChild(element, "ejb-link"));
   }		
    
   public void importJbossXml(Element element) throws DeploymentException {
      jndiName = getElementContent(getOptionalChild(element, "jndi-name"));
   }
	
   public void importJbossXml(String invokerBinding, Element element) throws DeploymentException 
   {
      String refJndiName = getElementContent(getOptionalChild(element, "jndi-name"));
      invokerMap.put(invokerBinding, refJndiName);
   }
	
   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------
    
   // Inner classes -------------------------------------------------
}
