/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.metadata;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Set;

import org.jboss.deployment.DeploymentException;
import org.jboss.invocation.InvocationType;
import org.jboss.security.AnybodyPrincipal;
import org.jboss.security.NobodyPrincipal;
import org.jboss.security.SimplePrincipal;
import org.jboss.mx.util.ObjectNameFactory;

import org.w3c.dom.Element;

/**
 * A common meta data class for the entity, message-driven and session beans.
 *
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @author <a href="mailto:criege@riege.com">Christian Riege</a>
 *
 * @version $Revision: 1.41.2.8 $
 */
public abstract class BeanMetaData
   extends MetaData
{
   // Constants -----------------------------------------------------

   public static final char SESSION_TYPE = 'S';
   public static final char ENTITY_TYPE = 'E';
   public static final char MDB_TYPE = 'M';
   public static final String LOCAL_INVOKER_PROXY_BINDING = "LOCAL";

   // Attributes ----------------------------------------------------
   private ApplicationMetaData application;

   // from ejb-jar.xml
   /** The ejb-name element specifies an enterprise bean's name. This name is
       assigned by the ejb-jar file producer to name the enterprise bean in
       the ejb-jar file's deployment descriptor. The name must be unique
       among the names of the enterprise beans in the same ejb-jar file.
   */
   private String ejbName;
   /** The home element contains the fully-qualified name of the enterprise
       bean's home interface. */
   private String homeClass;
   /** The remote element contains the fully-qualified name of the enterprise
       bean's remote interface. */
   private String remoteClass;
   /** The local-home element contains the fully-qualified name of the
       enterprise bean's local home interface. */
   private String localHomeClass;
   /** The local element contains the fully-qualified name of the enterprise
       bean's local interface */
   private String localClass;
   /** The ejb-class element contains the fully-qualified name of the
       enterprise bean's class. */
   private String ejbClass;
   /** The type of bean: ENTITY_TYPE, SESSION_TYPE, MDB_TYPE */
   protected char beanType;
   /** Is this bean's transactions managed by the container? */
   protected boolean containerManagedTx = true;

   /** The The env-entry element(s) contains the declaration of an enterprise
       bean's environment entry */
   private ArrayList environmentEntries = new ArrayList();
   /** The The ejb-ref element(s) for the declaration of a reference to an
       enterprise bean's home */
   private HashMap ejbReferences = new HashMap();
   /** The ejb-local-ref element(s) info */
   private HashMap ejbLocalReferences = new HashMap();
   /** The security-role-ref element(s) info */
   private ArrayList securityRoleReferences = new ArrayList();
   /** The security-idemtity element info */
   private SecurityIdentityMetaData securityIdentity = null;
   /** The resource-ref element(s) info */
   private HashMap resourceReferences = new HashMap();
   /** The resource-env-ref element(s) info */
   private HashMap resourceEnvReferences = new HashMap();
   /** The method attributes */
   private ArrayList methodAttributes = new ArrayList();
   private HashMap cachedMethodAttributes = new HashMap();
   /** The assembly-descriptor/method-permission element(s) info */
   private ArrayList permissionMethods = new ArrayList();
   /** The assembly-descriptor/container-transaction element(s) info */
   private ArrayList transactionMethods = new ArrayList();
   /** The assembly-descriptor/exclude-list method(s) */
   private ArrayList excludedMethods = new ArrayList();
   /** The invoker names to JNDI name mapping */
   protected HashMap invokerBindings = null;
   /** The cluster-config element info */
   private ClusterConfigMetaData clusterConfig = null;

   /** The JNDI name under with the home interface should be bound */
   private String jndiName;

   /** The JNDI name under with the local home interface should be bound */
   private String localJndiName;
   /** The container configuration name */
   protected String configurationName;
   /** The container configuration metadata */
   private ConfigurationMetaData configuration;
   /** The custom security proxy class */
   private String securityProxy;
   /** Is the bean marked as clustered */
   protected boolean clustered = false;
   /** Any object names for services the bean depends on */
   private Collection depends = new LinkedList();

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public BeanMetaData(ApplicationMetaData app, char beanType)
   {
      application = app;
      this.beanType = beanType;
   }

   // Public --------------------------------------------------------
   public boolean isSession()
   {
      return beanType == SESSION_TYPE;
   }

   public boolean isMessageDriven()
   {
      return beanType == MDB_TYPE;
   }

   public boolean isEntity()
   {
      return beanType == ENTITY_TYPE;
   }

   public String getHome()
   {
      return homeClass;
   }

   public String getRemote()
   {
      return remoteClass;
   }

   public String getLocalHome()
   {
      return localHomeClass;
   }

   public String getLocal()
   {
      return localClass;
   }

   public String getEjbClass()
   {
      return ejbClass;
   }

   public String getEjbName()
   {
      return ejbName;
   }

   public boolean isContainerManagedTx()
   {
      return containerManagedTx;
   }

   public boolean isBeanManagedTx()
   {
      return !containerManagedTx;
   }

   public Iterator getEjbReferences()
   {
      return ejbReferences.values().iterator();
   }

   public Iterator getEjbLocalReferences()
   {
      return ejbLocalReferences.values().iterator();
   }

   protected abstract void defaultInvokerBindings();

   public Iterator getInvokerBindings()
   {
      if( invokerBindings == null )
      {
         // See if there is a container default invoker name
         String defaultName = configuration.getDefaultInvokerName();
         if( defaultName != null )
         {
            invokerBindings = new HashMap();
            invokerBindings.put(defaultName, getJndiName());
         }
         else
         {
            // Use the hard-coded defaults
            defaultInvokerBindings();
         }
      }

      return invokerBindings.keySet().iterator();
   }

   public String getInvokerBinding(String invokerName)
   {
      if( invokerBindings == null )
      {
         defaultInvokerBindings();
      }

      return (String)invokerBindings.get(invokerName);
   }

   public EjbRefMetaData getEjbRefByName(String name)
   {
      return (EjbRefMetaData)ejbReferences.get(name);
   }

   public EjbLocalRefMetaData getEjbLocalRefByName(String name)
   {
      return (EjbLocalRefMetaData)ejbLocalReferences.get(name);
   }

   public Iterator getEnvironmentEntries()
   {
      return environmentEntries.iterator();
   }

   public Iterator getSecurityRoleReferences()
   {
      return securityRoleReferences.iterator();
   }

   public Iterator getResourceReferences()
   {
      return resourceReferences.values().iterator();
   }

   public Iterator getResourceEnvReferences()
   {
      return resourceEnvReferences.values().iterator();
   }

   public String getJndiName()
   {
      // jndiName may be set in jboss.xml
      if (jndiName == null)
      {
         jndiName = ejbName;
      }
      return jndiName;
   }

   /**
    * Gets the JNDI name under with the local home interface should be bound.
    * The default is local/&lt;ejbName&gt;
    */
   public String getLocalJndiName()
   {
      if (localJndiName == null)
      {
         localJndiName = "local/" + ejbName;
      }
      return localJndiName;
   }

   /**
    * Gets the container jndi name used in the object name
    */
   public String getContainerObjectNameJndiName()
   {
      return getHome() != null ? getJndiName() : getLocalJndiName();
   }

   public String getConfigurationName()
   {
      if (configurationName == null)
      {
         configurationName = getDefaultConfigurationName();
      }
      return configurationName;
   }

   public ConfigurationMetaData getContainerConfiguration()
   {
      if (configuration == null)
      {
         String configName = getConfigurationName();
         configuration = application.getConfigurationMetaDataByName(configName);
      }
      return configuration;
   }

   public String getSecurityProxy()
   {
      return securityProxy;
   }

   public SecurityIdentityMetaData getSecurityIdentityMetaData()
   {
      return securityIdentity;
   }

   public ApplicationMetaData getApplicationMetaData()
   {
      return application;
   }

   public abstract String getDefaultConfigurationName();

   public Iterator getTransactionMethods()
   {
      return transactionMethods.iterator();
   }

   public Iterator getPermissionMethods()
   {
      return permissionMethods.iterator();
   }

   public Iterator getExcludedMethods()
   {
      return excludedMethods.iterator();
   }

   public void addTransactionMethod( MethodMetaData method )
   {
      transactionMethods.add(method);
   }

   public void addPermissionMethod(MethodMetaData method)
   {
      // Insert unchecked methods into the front of the list to speed
      // up their validation
      if( method.isUnchecked() )
      {
         permissionMethods.add(0, method);
      }
      else
      {
         permissionMethods.add(method);
      }
   }

   public void addExcludedMethod(MethodMetaData method)
   {
      excludedMethods.add(method);
   }

   public byte getMethodTransactionType(String methodName, Class[] params,
      InvocationType iface)
   {
      // default value
      byte result = TX_UNKNOWN;

      Iterator iterator = getTransactionMethods();
      while( iterator.hasNext() )
      {
         MethodMetaData m = (MethodMetaData)iterator.next();
         if (m.patternMatches(methodName, params, iface))
         {
            result = m.getTransactionType();
            // if it is an exact match, break, if it is the wildcard
            // continue to look for a finer match
            if ( m.getMethodName().equals("*") == false )
            {
               break;
            }
         }
      }

      return result;
   }

   public Collection getDepends()
   {
      Collection allDepends = new LinkedList(depends);
      allDepends.addAll(getContainerConfiguration().getDepends());
      return allDepends;
   }

   /**
    * Checks meta data to obtain the Method Attributes of a bean's method:
    * method attributes are read-only, idempotent and potentially other
    * ones as well.
    * These jboss-specific method attributes are described in jboss.xml
    */
   private MethodAttributes methodAttributesForMethod(String methodName)
   {
      MethodAttributes ma =
         (MethodAttributes)cachedMethodAttributes.get(methodName);

      if(ma == null)
      {
         Iterator iterator = methodAttributes.iterator();
         while(iterator.hasNext() && ma == null)
         {
            ma = (MethodAttributes)iterator.next();
            if(!ma.patternMatches(methodName))
            {
               ma = null;
            }
         }
         if(ma == null)
         {
            ma = MethodAttributes.kDefaultMethodAttributes;
         }

         cachedMethodAttributes.put(methodName, ma);
      }
      return ma;
   }

   /**
    * Is this method a read-only method described in jboss.xml?
    */
   public boolean isMethodReadOnly(String methodName)
   {
      return methodAttributesForMethod(methodName).readOnly;
   }
   public boolean isMethodReadOnly(Method method)
   {
      if(method == null)
      {
         return false;
      }
      return methodAttributesForMethod(method.getName()).readOnly;
   }

   /**
    *  A somewhat tedious method that builds a Set<Principal> of the roles
    *  that have been assigned permission to execute the indicated method. The
    *  work performed is tedious because of the wildcard style of declaring
    *  method permission allowed in the ejb-jar.xml descriptor. This method is
    *  called by the Container.getMethodPermissions() when it fails to find the
    *  prebuilt set of method roles in its cache.
    *
    *  @return The Set<Principal> for the application domain roles that
    *     caller principal's are to be validated against.
    *  @see org.jboss.ejb.Container#getMethodPermissions(Method, boolean)
    */
   public Set getMethodPermissions(String methodName, Class[] params,
      InvocationType iface)
   {
      Set result = new HashSet();
      // First check the excluded method list as this takes priority over
      // all other assignments
      Iterator iterator = getExcludedMethods();
      while (iterator.hasNext())
      {
         MethodMetaData m = (MethodMetaData) iterator.next();
         if (m.patternMatches(methodName, params, iface))
         {
            /* No one is allowed to execute this method so add a role that
               fails to equate to any Principal or Principal name and return.
               We don't return null to differentiate between an explicit
               assignment of no access and no assignment information.
            */
            result.add(NobodyPrincipal.NOBODY_PRINCIPAL);
            return result;
         }
      }

      // Check the permissioned methods list
      iterator = getPermissionMethods();
      while (iterator.hasNext())
      {
         MethodMetaData m = (MethodMetaData) iterator.next();
         if (m.patternMatches(methodName, params, iface))
         {
            /* If this is an unchecked method anyone can access it so
               set the result set to a role that equates to any Principal or
               Principal name and return.
            */
            if (m.isUnchecked())
            {
               result.clear();
               result.add(AnybodyPrincipal.ANYBODY_PRINCIPAL);
               break;
            }
            // Else, add all roles
            else
            {
               Iterator rolesIterator = m.getRoles().iterator();
               while (rolesIterator.hasNext())
               {
                  String roleName = (String) rolesIterator.next();
                  result.add(new SimplePrincipal(roleName));
               }
            }
         }
      }

      // If no permissions were assigned to the method return null to indicate no access
      if( result.isEmpty() )
      {
         result = null;
      }

      return result;
   }

   // Cluster configuration methods
   public boolean isClustered()
   {
      return this.clustered;
   }

   public ClusterConfigMetaData getClusterConfigMetaData()
   {
	   if( clusterConfig == null )
	   {
		   clusterConfig = getContainerConfiguration().getClusterConfigMetaData();
         if( clusterConfig == null )
         {
            clusterConfig = new ClusterConfigMetaData();
         }
         /* All beans associated with a container are the same type
            so this can be done more than once without harm */
         clusterConfig.init(this);
	   }
	   return this.clusterConfig;
   }

   public void importEjbJarXml(Element element)
      throws DeploymentException
   {
      // set the ejb-name
      ejbName = getElementContent( getUniqueChild(element, "ejb-name") );

      // set the classes
      // Not for MessageDriven
      if (isMessageDriven() == false)
      {
         homeClass = getElementContent(getOptionalChild(element, "home"));
         remoteClass = getElementContent(getOptionalChild(element, "remote"));
         localHomeClass = getElementContent(getOptionalChild(element,
            "local-home"));
         localClass = getElementContent(getOptionalChild(element, "local"));
      }
      ejbClass = getElementContent(getUniqueChild(element, "ejb-class"));

      // set the environment entries
      Iterator iterator = getChildrenByTagName(element, "env-entry");

      while (iterator.hasNext())
      {
         Element envEntry = (Element)iterator.next();

         EnvEntryMetaData envEntryMetaData = new EnvEntryMetaData();
         envEntryMetaData.importEjbJarXml(envEntry);

         environmentEntries.add(envEntryMetaData);
      }

      // set the ejb references
      iterator = getChildrenByTagName(element, "ejb-ref");

      while (iterator.hasNext())
      {
         Element ejbRef = (Element) iterator.next();

         EjbRefMetaData ejbRefMetaData = new EjbRefMetaData();
         ejbRefMetaData.importEjbJarXml(ejbRef);

         ejbReferences.put(ejbRefMetaData.getName(), ejbRefMetaData);
      }

      // set the ejb local references
      iterator = getChildrenByTagName(element, "ejb-local-ref");

      while (iterator.hasNext())
      {
         Element ejbLocalRef = (Element) iterator.next();

         EjbLocalRefMetaData ejbLocalRefMetaData = new EjbLocalRefMetaData();
         ejbLocalRefMetaData.importEjbJarXml(ejbLocalRef);

         ejbLocalReferences.put(ejbLocalRefMetaData.getName(),
            ejbLocalRefMetaData);
      }

      // set the security roles references
      iterator = getChildrenByTagName(element, "security-role-ref");

      while (iterator.hasNext())
      {
         Element secRoleRef = (Element) iterator.next();

         SecurityRoleRefMetaData securityRoleRefMetaData =
            new SecurityRoleRefMetaData();
         securityRoleRefMetaData.importEjbJarXml(secRoleRef);

         securityRoleReferences.add(securityRoleRefMetaData);
      }

      // The security-identity element
      Element securityIdentityElement = getOptionalChild(element,
         "security-identity");
      if (securityIdentityElement != null)
      {
         securityIdentity = new SecurityIdentityMetaData();
         securityIdentity.importEjbJarXml(securityIdentityElement);
      }

      // set the resource references
      iterator = getChildrenByTagName(element, "resource-ref");

      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();

         ResourceRefMetaData resourceRefMetaData = new ResourceRefMetaData();
         resourceRefMetaData.importEjbJarXml(resourceRef);

         resourceReferences.put(resourceRefMetaData.getRefName(),
            resourceRefMetaData);
      }

      // Parse the resource-env-ref elements
      iterator = getChildrenByTagName(element, "resource-env-ref");
      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();	
         ResourceEnvRefMetaData refMetaData = new ResourceEnvRefMetaData();
         refMetaData.importEjbJarXml(resourceRef);
         resourceEnvReferences.put(refMetaData.getRefName(), refMetaData);
      }
   }

   public void importJbossXml(Element element) throws DeploymentException
   {
      // we must not set defaults here, this might never be called

      // set the jndi name, (optional)
      jndiName = getElementContent(getOptionalChild(element, "jndi-name"));

      // set the JNDI name under with the local home interface should be
      // bound (optional)
      localJndiName = getElementContent(getOptionalChild(element,
         "local-jndi-name"));

      // set the configuration (optional)
      configurationName = getElementContent(getOptionalChild(element,
         "configuration-name"));
      if( configurationName != null && getApplicationMetaData().getConfigurationMetaDataByName(configurationName) == null)
      {
         throw new DeploymentException("configuration '" + configurationName
            + "' not found in standardjboss.xml or jboss.xml");
      }

      // Get the security proxy
      securityProxy = getElementContent(getOptionalChild(element,
         "security-proxy"), securityProxy);

      // update the resource references (optional)
      Iterator iterator = getChildrenByTagName(element, "resource-ref");
      while (iterator.hasNext()) {
         Element resourceRef = (Element)iterator.next();
         String resRefName = getElementContent(getUniqueChild(resourceRef,
            "res-ref-name"));
         ResourceRefMetaData resourceRefMetaData =
            (ResourceRefMetaData)resourceReferences.get(resRefName);

         if (resourceRefMetaData == null)
         {
            throw new DeploymentException("resource-ref " + resRefName
               + " found in jboss.xml but not in ejb-jar.xml");
         }
         resourceRefMetaData.importJbossXml(resourceRef);
      }

      // Set the resource-env-ref deployed jndi names
      iterator = getChildrenByTagName(element, "resource-env-ref");
      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();	
         String resRefName = getElementContent(getUniqueChild(resourceRef,
            "resource-env-ref-name"));
         ResourceEnvRefMetaData refMetaData =
            (ResourceEnvRefMetaData) resourceEnvReferences.get(resRefName);
         if (refMetaData == null)
         {
            throw new DeploymentException("resource-env-ref " + resRefName
               + " found in jboss.xml but not in ejb-jar.xml");
         }
         refMetaData.importJbossXml(resourceRef);
      }

      // set the external ejb-references (optional)
      iterator = getChildrenByTagName(element, "ejb-ref");
      while (iterator.hasNext())
      {
         Element ejbRef = (Element)iterator.next();
         String ejbRefName = getElementContent(getUniqueChild(ejbRef,
            "ejb-ref-name"));
         EjbRefMetaData ejbRefMetaData = getEjbRefByName(ejbRefName);
         if (ejbRefMetaData == null)
         {
            throw new DeploymentException("ejb-ref " + ejbRefName
               + " found in jboss.xml but not in ejb-jar.xml");
         }
         ejbRefMetaData.importJbossXml(ejbRef);
      }


      //handle the ejb-local-ref elements
      iterator = getChildrenByTagName(element, "ejb-local-ref");
      while (iterator.hasNext())
      {
          Element ejbLocalRef = (Element)iterator.next();
          String ejbLocalRefName = getElementContent(getUniqueChild(ejbLocalRef,
                                                               "ejb-ref-name"));

          EjbLocalRefMetaData ejbLocalRefMetaData = getEjbLocalRefByName(ejbLocalRefName);
          if (ejbLocalRefMetaData == null)
          {
              throw new DeploymentException("ejb-local-ref " + ejbLocalRefName
                                            + " found in jboss.xml but not in ejb-jar.xml");
          }
         ejbLocalRefMetaData.importJbossXml(ejbLocalRef); 
      }



      
      // Method attributes of the bean
      Element mas = getOptionalChild(element, "method-attributes");
      if(mas != null)
      {
         // read in the read-only methods
         iterator = getChildrenByTagName(mas, "method");
         while (iterator.hasNext())
         {
            MethodAttributes ma = new MethodAttributes();
            Element maNode = (Element)iterator.next();
            ma.pattern = getElementContent(getUniqueChild(maNode,
               "method-name"));
            ma.readOnly = getOptionalChildBooleanContent(maNode,
               "read-only");
            ma.idempotent = getOptionalChildBooleanContent(maNode,
               "idempotent");
            methodAttributes.add(ma);
         }
      }

      // Invokers
      // If no invoker bindings have been defined they will be defined
      // in EntityMetaData, or SessionMetaData
      Element inv = getOptionalChild(element, "invoker-bindings");
      if(inv != null)
      {
         // read in the read-only methods
         iterator = getChildrenByTagName(inv, "invoker");
         invokerBindings = new HashMap();
         while (iterator.hasNext())
         {
            Element node = (Element)iterator.next();
            String invokerBindingName = getUniqueChildContent(node,
               "invoker-proxy-binding-name");
            String jndiBinding = getOptionalChildContent(node, "jndi-name");

            if( jndiBinding == null )
            {
               jndiBinding = getJndiName(); // default to jndiName
            }
            invokerBindings.put(invokerBindingName, jndiBinding);

            // set the external ejb-references (optional)
            Iterator ejbrefiterator = getChildrenByTagName(node, "ejb-ref");
            while (ejbrefiterator.hasNext())
            {
               Element ejbRef = (Element)ejbrefiterator.next();
               String ejbRefName = getElementContent(getUniqueChild(ejbRef,
                  "ejb-ref-name"));
               EjbRefMetaData ejbRefMetaData = getEjbRefByName(ejbRefName);
               if (ejbRefMetaData == null)
               {
                  throw new DeploymentException("ejb-ref " + ejbRefName
                     + " found in jboss.xml but not in ejb-jar.xml");
               }
               ejbRefMetaData.importJbossXml(invokerBindingName, ejbRef);
            }
         }
      }

      // Determine if the bean is to be deployed in the cluster (more
      // advanced config will be added in the future)
      String clusteredElt = getElementContent(getOptionalChild(element,
         "clustered"), (clustered? "True" : "False"));
      clustered = clusteredElt.equalsIgnoreCase ("True");

      Element clusterConfigElement = getOptionalChild(element,
         "cluster-config");
      if (clusterConfigElement != null)
      {
         this.clusterConfig = new ClusterConfigMetaData();
         clusterConfig.init(this);
         clusterConfig.importJbossXml(clusterConfigElement);
      }

      //Get depends object names
      for( Iterator dependsElements = getChildrenByTagName(element, "depends");
         dependsElements.hasNext();)
      {
         Element dependsElement = (Element)dependsElements.next();
         String dependsName = getElementContent(dependsElement);
         depends.add(ObjectNameFactory.create(dependsName));
      } // end of for ()

   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
/*
vim:ts=3:sw=3:et
*/
