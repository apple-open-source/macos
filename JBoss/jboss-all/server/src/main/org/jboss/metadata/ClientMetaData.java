package org.jboss.metadata;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.w3c.dom.Element;

/** The metdata data from a j2ee application-client.xml descriptor
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class ClientMetaData
{
   private static Logger log = Logger.getLogger(ClientMetaData.class);

   /** The application-client/display-name */
   private String displayName;
   /** The location for the server side client context ENC bindings */
   private String jndiName;
   /** An ArrayList<EnvEntryMetaData> for the env-entry element(s) */
   private ArrayList environmentEntries = new ArrayList();
   /** A HashMap<EjbRefMetaData> for the ejb-ref element(s) */
   private HashMap ejbReferences = new HashMap();
   /** A  HashMap<ResourceRefMetaData> resource-ref element(s) info */
   private HashMap resourceReferences = new HashMap();
   /** A  HashMap<ResourceEnvRefMetaData> resource-env-ref element(s) info */
   private HashMap resourceEnvReferences = new HashMap();
   /** The JAAS callback handler */
   private String callbackHandler;

   /** The application-client/display-name
    * @return application-client/display-name value
    */ 
   public String getDisplayName()
   {
      return displayName;
   }

   /** The location for the server side client context ENC bindings
    * @return the JNDI name for the server side client context ENC bindings. This
    * is either the jboss-client/jndi-name or the application-client/display-name
    * value.
    */ 
   public String getJndiName()
   {
      String name = jndiName;
      if( name == null )
         name = displayName;
      return name;
   }

   /**
    * @return ArrayList<EnvEntryMetaData>
    */ 
   public ArrayList getEnvironmentEntries()
   {
      return environmentEntries;
   }
   /**
    * @return HashMap<EjbRefMetaData>
    */ 
   public HashMap getEjbReferences()
   {
      return ejbReferences;
   }
   /**
    * @return HashMap<ResourceRefMetaData>
    */ 
   public HashMap getResourceReferences()
   {
      return resourceReferences;
   }
   /**
    * @return HashMap<ResourceEnvRefMetaData>
    */
   public HashMap getResourceEnvReferences()
   {
      return resourceEnvReferences;
   }
   /** 
    * @return The CallbackHandler if defined, null otherwise
    */ 
   public String getCallbackHandler()
   {
      return callbackHandler;
   }

   public void importClientXml(Element element)
      throws DeploymentException
   {
      displayName = MetaData.getUniqueChildContent(element, "display-name");

      // set the environment entries
      Iterator iterator = MetaData.getChildrenByTagName(element, "env-entry");

      while (iterator.hasNext())
      {
         Element envEntry = (Element) iterator.next();

         EnvEntryMetaData envEntryMetaData = new EnvEntryMetaData();
         envEntryMetaData.importEjbJarXml(envEntry);

         environmentEntries.add(envEntryMetaData);
      }

      // set the ejb references
      iterator = MetaData.getChildrenByTagName(element, "ejb-ref");

      while (iterator.hasNext())
      {
         Element ejbRef = (Element) iterator.next();

         EjbRefMetaData ejbRefMetaData = new EjbRefMetaData();
         ejbRefMetaData.importEjbJarXml(ejbRef);

         ejbReferences.put(ejbRefMetaData.getName(), ejbRefMetaData);
      }

      // The callback-handler element
      Element callbackElement = MetaData.getOptionalChild(element,
         "callback-handler");
      if (callbackElement != null)
      {
         callbackHandler = MetaData.getElementContent(callbackElement);
      }

      // set the resource references
      iterator = MetaData.getChildrenByTagName(element, "resource-ref");
      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();

         ResourceRefMetaData resourceRefMetaData = new ResourceRefMetaData();
         resourceRefMetaData.importEjbJarXml(resourceRef);

         resourceReferences.put(resourceRefMetaData.getRefName(),
            resourceRefMetaData);
      }

      // Parse the resource-env-ref elements
      iterator = MetaData.getChildrenByTagName(element, "resource-env-ref");
      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();
         ResourceEnvRefMetaData refMetaData = new ResourceEnvRefMetaData();
         refMetaData.importEjbJarXml(resourceRef);
         resourceEnvReferences.put(refMetaData.getRefName(), refMetaData);
      }
   }

   public void importJbossClientXml(Element element) throws DeploymentException
   {
      jndiName = MetaData.getOptionalChildContent(element, "jndi-name");

      // Get the JNDI names of ejb-refs
      Iterator iterator = MetaData.getChildrenByTagName(element, "ejb-ref");
      while (iterator.hasNext())
      {
         Element ejbRef = (Element) iterator.next();
         String ejbRefName = MetaData.getElementContent(
            MetaData.getUniqueChild(ejbRef, "ejb-ref-name"));
         EjbRefMetaData ejbRefMetaData = (EjbRefMetaData) ejbReferences.get(ejbRefName);
         if (ejbRefMetaData == null)
         {
            throw new DeploymentException("ejb-ref " + ejbRefName
               + " found in jboss-client.xml but not in application-client.xml");
         }
         ejbRefMetaData.importJbossXml(ejbRef);
      }

      // Get the JNDI name binding for resource-refs
      iterator = MetaData.getChildrenByTagName(element, "resource-ref");
      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();
         String resRefName = MetaData.getElementContent(
            MetaData.getUniqueChild(resourceRef, "res-ref-name"));
         ResourceRefMetaData resourceRefMetaData =
            (ResourceRefMetaData) resourceReferences.get(resRefName);
         if (resourceRefMetaData == null)
         {
            throw new DeploymentException("resource-ref " + resRefName
               + " found in jboss-client.xml but not in application-client.xml");
         }
         resourceRefMetaData.importJbossXml(resourceRef);
      }

      // Get the JNDI name binding resource-env-refs
      iterator = MetaData.getChildrenByTagName(element, "resource-env-ref");
      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();
         String resRefName = MetaData.getElementContent(
            MetaData.getUniqueChild(resourceRef, "resource-env-ref-name"));
         ResourceEnvRefMetaData refMetaData =
            (ResourceEnvRefMetaData) resourceEnvReferences.get(resRefName);
         if (refMetaData == null)
         {
            throw new DeploymentException("resource-env-ref " + resRefName
               + " found in jboss-client.xml but not in application-client.xml");
         }
         refMetaData.importJbossXml(resourceRef);
      }

   }

}
