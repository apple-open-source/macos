/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.metadata;

import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;

import org.w3c.dom.DocumentType;
import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

/**
 * The top level meta data from the jboss.xml and ejb-jar.xml descriptor.
 *
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @author <a href="mailto:criege@riege.com">Christian Riege</a>
 *
 * @version $Revision: 1.29.2.3 $
 */
public class ApplicationMetaData
   extends MetaData
{
   public static final int EJB_1x = 1;
   public static final int EJB_2x = 2;

   private URL url;

   // verion of the dtd used to create ejb-jar.xml
   private int ejbVersion;

   private ArrayList beans = new ArrayList();

   /**
    * List of relations in this application.
    * Items are instance of RelationMetaData.
    */
   private ArrayList relationships = new ArrayList();

   private ArrayList securityRoles = new ArrayList();
   private HashMap configurations = new HashMap();
   private HashMap invokerBindings = new HashMap();
   private HashMap resources = new HashMap();
   private HashMap plugins = new HashMap();
   /** The user defined JMX name for the EJBModule */
   private String jmxName;
   /** The security-domain value assigned to the application */
   private String securityDomain;
   /** The  unauthenticated-principal value assigned to the application */
   private String  unauthenticatedPrincipal;
   private boolean enforceEjbRestrictions;

   public ApplicationMetaData(URL u)
   {
      url = u;
   }

   public ApplicationMetaData()
   {
   }

   public URL getUrl()
   {
      return url;
   }

   public void setUrl(URL u)
   {
      url = u;
   }

   public boolean isEJB1x()
   {
      return ejbVersion == 1;
   }

   public boolean isEJB2x()
   {
      return ejbVersion == 2;
   }

   /**
    */
   public Iterator getEnterpriseBeans()
   {
      return beans.iterator();
   }

   /**
    * Get an EJB by its declared &lt;ejb-name&gt; tag
    *
    * @param ejbName EJB to return
    *
    * @return BeanMetaData pertaining to the given ejb-name,
    *   <code>null</code> if none found
    */
   public BeanMetaData getBeanByEjbName( String ejbName )
   {
      Iterator iterator = getEnterpriseBeans();
      while (iterator.hasNext())
      {
         BeanMetaData current = (BeanMetaData) iterator.next();
         if( current.getEjbName().equals(ejbName) )
         {
            return current;
         }
      }

      // not found
      return null;
   }

   /**
    * Get the container managed relations in this application.
    * Items are instance of RelationMetaData.
    */
   public Iterator getRelationships()
   {
      return relationships.iterator();
   }

   public Iterator getConfigurations()
   {
      return configurations.values().iterator();
   }

   public ConfigurationMetaData getConfigurationMetaDataByName(String name)
   {
      return (ConfigurationMetaData)configurations.get(name);
   }

   public Iterator getInvokerProxyBindings()
   {
      return invokerBindings.values().iterator();
   }

   public InvokerProxyBindingMetaData getInvokerProxyBindingMetaDataByName(
      String name)
   {
      return (InvokerProxyBindingMetaData)invokerBindings.get(name);
   }

   public String getResourceByName(String name)
   {
      // if not found, the container will use default
      return (String)resources.get(name);
   }

   public void addPluginData(String pluginName, Object pluginData)
   {
      plugins.put(pluginName, pluginData);
   }

   public Object getPluginData(String pluginName)
   {
      return plugins.get(pluginName);
   }

   public String getJmxName()
   {
      return jmxName;
   }

   public String getSecurityDomain()
   {
      return securityDomain;
   }

   public String getUnauthenticatedPrincipal()
   {
      return unauthenticatedPrincipal;
   }

   public boolean getEnforceEjbRestrictions()
   {
      return enforceEjbRestrictions;
   }

   /**
    * Import data provided by ejb-jar.xml
    *
    * @throws DeploymentException When there was an error encountered
    *         while parsing ejb-jar.xml
    */
   public void importEjbJarXml( Element element )
      throws DeploymentException
   {
      // EJB version is determined by the doc type that was used to
      // verify the ejb-jar.xml.
      DocumentType docType = element.getOwnerDocument().getDoctype();

      if( docType == null )
      {
         // No good, EJB 1.1/2.0 requires a DOCTYPE declaration
         throw new DeploymentException( "ejb-jar.xml must define a " +
            "valid DOCTYPE!" );
      }

      String publicId = docType.getPublicId();
      if( publicId == null )
      {
         // We need a public Id
         throw new DeploymentException( "The DOCTYPE declaration in " +
            "ejb-jar.xml must define a PUBLIC id" );
      }

      // Check for a known public Id
      if( publicId.startsWith(
         "-//Sun Microsystems, Inc.//DTD Enterprise JavaBeans 2.0") )
      {
         ejbVersion = 2;
      }
      else if( publicId.startsWith(
         "-//Sun Microsystems, Inc.//DTD Enterprise JavaBeans 1.1") )
      {
         ejbVersion = 1;
      }
      else
      {
         // Unknown
         throw new DeploymentException( "Unknown PUBLIC id in " + 
            "ejb-jar.xml: " + publicId );
      }

      // find the beans
      Element enterpriseBeans = getUniqueChild(element, "enterprise-beans");

      // Entity Beans
      HashMap schemaNameMap = new HashMap();
      Iterator iterator = getChildrenByTagName(enterpriseBeans, "entity");
      while (iterator.hasNext())
      {
         Element currentEntity = (Element)iterator.next();
         EntityMetaData entityMetaData = new EntityMetaData(this);
         try
         {
            entityMetaData.importEjbJarXml(currentEntity);
         }
         catch (DeploymentException e)
         {
            throw new DeploymentException( "Error in ejb-jar.xml " +
               "for Entity Bean " + entityMetaData.getEjbName() + ": " +
               e.getMessage());
         }

         // Ensure unique-ness of <abstract-schema-name>
         String abstractSchemaName = entityMetaData.getAbstractSchemaName();
         if( abstractSchemaName != null )
         {
            if( schemaNameMap.containsKey(abstractSchemaName) )
            {
               //
               throw new DeploymentException( entityMetaData.getEjbName() +
                  ": Duplicate abstract-schema name '" + abstractSchemaName +
                  "'. Already defined for Entity '" +
                  ((EntityMetaData)schemaNameMap.get(abstractSchemaName)).getEjbName() + "'." );
            }
            schemaNameMap.put( abstractSchemaName, entityMetaData );
         }

         beans.add( entityMetaData );
      }

      // Session Beans
      iterator = getChildrenByTagName(enterpriseBeans, "session");
      while (iterator.hasNext())
      {
         Element currentSession = (Element)iterator.next();
         SessionMetaData sessionMetaData = new SessionMetaData(this);
         try
         {
            sessionMetaData.importEjbJarXml(currentSession);
         }
         catch (DeploymentException e)
         {
            throw new DeploymentException( "Error in ejb-jar.xml for " +
               "Session Bean " + sessionMetaData.getEjbName() + ": " +
               e.getMessage() );
         }
         beans.add(sessionMetaData);
      }

      // Message Driven Beans
      iterator = getChildrenByTagName(enterpriseBeans, "message-driven");
      while (iterator.hasNext())
      {
         Element currentMessageDriven = (Element)iterator.next();
         MessageDrivenMetaData messageDrivenMetaData =
            new MessageDrivenMetaData( this );

         try
         {
            messageDrivenMetaData.importEjbJarXml( currentMessageDriven );
         }
         catch (DeploymentException e)
         {
            throw new DeploymentException( "Error in ejb-jar.xml for " +
               "Message Driven Bean " +
               messageDrivenMetaData.getEjbName() + ": " + e.getMessage());
         }
         beans.add(messageDrivenMetaData);
      }

      // Enforce unique-ness of declared ejb-name Elements
      Set ejbNames = new HashSet();
      Iterator beanIt = beans.iterator();
      while( beanIt.hasNext() )
      {
         BeanMetaData bmd = (BeanMetaData)beanIt.next();

         String beanName = bmd.getEjbName();
         if( ejbNames.contains(beanName) )
         {
            throw new DeploymentException( "Duplicate definition of an " +
               "EJB with name '" + beanName + "'." );
         }

         ejbNames.add( beanName );
      }

      // Relationships
      Element relationshipsElement = getOptionalChild(element,
         "relationships");
      if(relationshipsElement != null)
      {
         // used to assure that a relationship name is not reused
         Set relationNames = new HashSet();

         iterator = getChildrenByTagName(relationshipsElement,
            "ejb-relation");
         while(iterator.hasNext())
         {
            Element relationElement = (Element)iterator.next();
            RelationMetaData relationMetaData = new RelationMetaData();
            try
            {
               relationMetaData.importEjbJarXml(relationElement);
            }
            catch (DeploymentException e)
            {
               throw new DeploymentException( "Error in ejb-jar.xml " +
                  "for relation " + relationMetaData.getRelationName() +
                  ": " + e.getMessage() );
            }

            // if the relationship has a name, assure that it has not
            // already been used
            String relationName = relationMetaData.getRelationName();
            if( relationName != null )
            {
               if( relationNames.contains(relationName) )
               {
                  throw new DeploymentException("ejb-relation-name must " +
                  "be unique in ejb-jar.xml file: ejb-relation-name is " +
                  relationName );
               }
               relationNames.add( relationName );
            }

            relationships.add(relationMetaData);
         }
      }

      // read the assembly descriptor (optional)
      Element assemblyDescriptor = getOptionalChild( element,
         "assembly-descriptor");
      if( assemblyDescriptor != null )
      {
         // set the security roles (optional)
         iterator = getChildrenByTagName(assemblyDescriptor, "security-role");
         while (iterator.hasNext())
         {
            Element securityRole = (Element)iterator.next();
            try
            {
               String role = getElementContent( getUniqueChild(securityRole,
                  "role-name") );
               securityRoles.add(role);
            }
            catch (DeploymentException e)
            {
               throw new DeploymentException( "Error in ejb-jar.xml " +
                  "for security-role: " + e.getMessage() );
            }
         }

         // set the method permissions (optional)
         iterator = getChildrenByTagName(assemblyDescriptor,
            "method-permission");
         try
         {
            while (iterator.hasNext())
            {
               Element methodPermission = (Element)iterator.next();
               // Look for the unchecked element
               Element unchecked = getOptionalChild(methodPermission,
                  "unchecked");

               boolean isUnchecked = false;
               Set roles = null;
               if( unchecked != null )
               {
                  isUnchecked = true;
               }
               else
               {
                  // Get the role-name elements
                  roles = new HashSet();
                  Iterator rolesIterator = getChildrenByTagName(
                     methodPermission, "role-name");
                  while( rolesIterator.hasNext() )
                  {
                     roles.add(getElementContent(
                        (Element)rolesIterator.next()) );
                  }
                  if( roles.size() == 0 )
                     throw new DeploymentException("An unchecked " +
                        "element or one or more role-name elements " +
                        "must be specified in method-permission");
               }

               // find the methods
               Iterator methods = getChildrenByTagName(methodPermission,
                  "method");
               while (methods.hasNext())
               {
                  // load the method
                  MethodMetaData method = new MethodMetaData();
                  method.importEjbJarXml((Element)methods.next());
                  if( isUnchecked )
                  {
                     method.setUnchecked();
                  }
                  else
                  {
                     method.setRoles(roles);
                  }

                  // give the method to the right bean
                  BeanMetaData bean = getBeanByEjbName(method.getEjbName());
                  if( bean == null )
                  {
                     throw new DeploymentException( method.getEjbName() +
                        " doesn't exist" );
                  }
                  bean.addPermissionMethod(method);
               }
            }
         }
         catch (DeploymentException e)
         {
            throw new DeploymentException( "Error in ejb-jar.xml, " +
               "in method-permission: " + e.getMessage() );
         }

         // set the container transactions (optional)
         iterator = getChildrenByTagName(assemblyDescriptor,
            "container-transaction");
         try
         {
            while (iterator.hasNext())
            {
               Element containerTransaction = (Element)iterator.next();

               // find the type of the transaction
               byte transactionType;
               String type = getElementContent( getUniqueChild(
                  containerTransaction, "trans-attribute") );

               if( type.equalsIgnoreCase("NotSupported") ||
                  type.equalsIgnoreCase("Not_Supported") )
               {
                  transactionType = TX_NOT_SUPPORTED;
               }
               else if( type.equalsIgnoreCase("Supports") )
               {
                  transactionType = TX_SUPPORTS;
               }
               else if( type.equalsIgnoreCase("Required") )
               {
                  transactionType = TX_REQUIRED;
               }
               else if( type.equalsIgnoreCase("RequiresNew") ||
                  type.equalsIgnoreCase("Requires_New") )
               {
                  transactionType = TX_REQUIRES_NEW;
               }
               else if( type.equalsIgnoreCase("Mandatory") )
               {
                  transactionType = TX_MANDATORY;
               }
               else if( type.equalsIgnoreCase("Never") )
               {
                  transactionType = TX_NEVER;
               }
               else
               {
                  throw new DeploymentException( "invalid " +
                     "<transaction-attribute> : " + type);
               }

               // find the methods
               Iterator methods = getChildrenByTagName(
                  containerTransaction, "method" );
               while (methods.hasNext())
               {
                  // load the method
                  MethodMetaData method = new MethodMetaData();
                  method.importEjbJarXml((Element)methods.next());
                  method.setTransactionType(transactionType);

                  // give the method to the right bean
                  BeanMetaData bean = getBeanByEjbName(method.getEjbName());
                  if( bean == null )
                  {
                     throw new DeploymentException( "bean " +
                        method.getEjbName() + " doesn't exist" );
                  }
                  bean.addTransactionMethod(method);
               }
            }
         }
         catch (DeploymentException e)
         {
            throw new DeploymentException( "Error in ejb-jar.xml, " +
               "in <container-transaction>: " + e.getMessage());
         }

         // Get the exclude-list methods
         Element excludeList = getOptionalChild(assemblyDescriptor,
            "exclude-list");
         if( excludeList != null )
         {
            iterator = getChildrenByTagName(excludeList, "method");
            while (iterator.hasNext())
            {
               Element methodInf = (Element) iterator.next();
               // load the method
               MethodMetaData method = new MethodMetaData();
               method.importEjbJarXml(methodInf);
               method.setExcluded();

               // give the method to the right bean
               BeanMetaData bean = getBeanByEjbName(method.getEjbName());
               if (bean == null)
               {
                  throw new DeploymentException( "bean " +
                     method.getEjbName() + " doesn't exist" );
               }
               bean.addExcludedMethod(method);
            }
         }
      }
   }

   public void importJbossXml(Element element)
      throws DeploymentException
   {
      Iterator iterator;

      // all the tags are optional

      // Get the enforce-ejb-restrictions
      Element enforce = getOptionalChild(element, "enforce-ejb-restrictions");
      if( enforce != null )
      {
         String tmp = getElementContent(enforce);
         enforceEjbRestrictions = Boolean.valueOf(tmp).booleanValue();
      }

      // Get any user defined JMX name
      Element jmxNameElement = getOptionalChild(element,
         "jmx-name");
      if( jmxNameElement != null )
      {
         jmxName = getElementContent(jmxNameElement);
      }

      // Get the security domain name
      Element securityDomainElement = getOptionalChild(element,
         "security-domain");
      if( securityDomainElement != null )
      {
         securityDomain = getElementContent(securityDomainElement);
      }

      // Get the unauthenticated-principal name
      Element unauth = getOptionalChild(element,
         "unauthenticated-principal");
      if( unauth != null )
      {
         unauthenticatedPrincipal = getElementContent(unauth);
      }

      // find the invoker configurations
      Element invokerConfs = getOptionalChild(element,
         "invoker-proxy-bindings");
      if (invokerConfs != null)
      {
         iterator = getChildrenByTagName(invokerConfs,
            "invoker-proxy-binding");

         while (iterator.hasNext())
         {
            Element invoker = (Element)iterator.next();
            String invokerName = getElementContent(getUniqueChild(
               invoker, "name"));

            // find the configuration if it has already been defined
            // (allow jboss.xml to modify a standard conf)
            InvokerProxyBindingMetaData invokerMetaData =
               getInvokerProxyBindingMetaDataByName(invokerName);

            // create it if necessary
            if (invokerMetaData == null)
            {
               invokerMetaData = new InvokerProxyBindingMetaData(invokerName);
               invokerBindings.put(invokerName, invokerMetaData);
            }

            try
            {
               invokerMetaData.importJbossXml(invoker);
            }
            catch (DeploymentException e)
            {
               throw new DeploymentException( "Error in jboss.xml " +
                  "for invoker-proxy-binding " + invokerMetaData.getName() +
                  ": " + e.getMessage() );
            }
         }
      }

      // find the container configurations (we need them first to use
      // them in the beans)
      Element confs = getOptionalChild(element, "container-configurations");
      if (confs != null)
      {
         iterator = getChildrenByTagName(confs, "container-configuration");

         while (iterator.hasNext())
         {
            Element conf = (Element)iterator.next();
            String confName = getElementContent(getUniqueChild(conf,
               "container-name"));
            String parentConfName = conf.getAttribute("extends");
            if( parentConfName != null && parentConfName.trim().length() == 0 )
            {
               parentConfName = null;
            }

            // Allow the configuration to inherit from a standard
            // configuration. This is determined by looking for a
            // configuration matching the name given by the extends
            // attribute, or if extends was not specified, an
            // existing configuration with the same.
            ConfigurationMetaData configurationMetaData = null;
            if( parentConfName != null )
            {
               configurationMetaData = getConfigurationMetaDataByName(
                  parentConfName);
               if( configurationMetaData == null )
               {
                  throw new DeploymentException( "Failed to find " +
                     "parent config=" + parentConfName );
               }

               // Make a copy of the existing configuration
               configurationMetaData =
                  (ConfigurationMetaData) configurationMetaData.clone();
               configurations.put( confName, configurationMetaData );
            }

            if( configurationMetaData == null )
            {
               configurationMetaData =
                  getConfigurationMetaDataByName(confName);
            }

            // Create a new configuration if none was found
            if( configurationMetaData == null )
            {
               configurationMetaData = new ConfigurationMetaData(confName);
               configurations.put(confName, configurationMetaData);
            }

            try
            {
               configurationMetaData.importJbossXml(conf);
            } catch (DeploymentException e)
            {
               throw new DeploymentException( "Error in jboss.xml " +
                  "for container-configuration " +
                  configurationMetaData.getName() + ": " + e.getMessage());
            }
         }
      }

      // update the enterprise beans
      Element entBeans = getOptionalChild(element, "enterprise-beans");
      if( entBeans != null )
      {
         String ejbName = null;
         try
         {
            // Entity Beans
            iterator = getChildrenByTagName( entBeans, "entity" );
            while( iterator.hasNext() )
            {
               Element bean = (Element) iterator.next();
               ejbName = getElementContent(getUniqueChild(bean, "ejb-name"));
               BeanMetaData beanMetaData = getBeanByEjbName(ejbName);
               if (beanMetaData == null)
               {
                  throw new DeploymentException("found in jboss.xml " +
                     "but not in ejb-jar.xml");
               }
               beanMetaData.importJbossXml(bean);
            }

            // Session Beans
            iterator = getChildrenByTagName(entBeans, "session");
            while (iterator.hasNext())
            {
               Element bean = (Element) iterator.next();
               ejbName = getElementContent(getUniqueChild(bean, "ejb-name"));
               BeanMetaData beanMetaData = getBeanByEjbName(ejbName);
               if (beanMetaData == null)
               {
                  throw new DeploymentException("found in jboss.xml " +
                     "but not in ejb-jar.xml");
               }
               beanMetaData.importJbossXml(bean);
            }

            // Message Driven Beans
            iterator = getChildrenByTagName(entBeans, "message-driven");
            while (iterator.hasNext())
            {
               Element bean = (Element) iterator.next();
               ejbName = getElementContent(getUniqueChild(bean, "ejb-name"));
               BeanMetaData beanMetaData = getBeanByEjbName(ejbName);
               if (beanMetaData == null)
               {
                  throw new DeploymentException("found in jboss.xml " +
                     "but not in ejb-jar.xml");
               }
               beanMetaData.importJbossXml(bean);
            }
         }
         catch (DeploymentException e)
         {
            throw new DeploymentException( "Error in jboss.xml for " +
               "Bean " + ejbName + ": " + e.getMessage() );
         }
      }

      // set the resource managers
      Element resmans = getOptionalChild(element, "resource-managers");
      if( resmans != null )
      {
         iterator = getChildrenByTagName(resmans, "resource-manager");
         try
         {
            while (iterator.hasNext())
            {
               Element resourceManager = (Element)iterator.next();
               String resName = getElementContent(getUniqueChild(
                  resourceManager, "res-name"));

               String jndi = getElementContent(getOptionalChild(
                  resourceManager, "res-jndi-name"));

               String url = getElementContent(getOptionalChild(
                  resourceManager, "res-url"));

               if( jndi != null && url == null )
               {
                  resources.put(resName, jndi);
               }
               else if( jndi == null && url != null )
               {
                  resources.put(resName, url);
               }
               else
               {
                  throw new DeploymentException( resName +
                     " : expected res-url or res-jndi-name tag" );
               }
            }
         }
         catch (DeploymentException e)
         {
            throw new DeploymentException( "Error in jboss.xml, in " +
               "resource-manager: " + e.getMessage() );
         }
      }
   }

}
/*
vim:ts=3:sw=3:et
*/
