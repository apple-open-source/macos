/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;
import java.lang.reflect.Method;

import javax.management.Notification;
import javax.management.ObjectName;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;
import org.jboss.resource.RARMetaData;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.util.Classes;
import org.jboss.util.Strings;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

/**
 * The RARDeployment mbean manages instantiation and configuration of a ManagedConnectionFactory instance. It is intended to be configured primarily by xslt transformation of the ra.xml from a jca adapter. Until that is implemented, it uses the old RARDeployment and RARDeployer mechanism to obtain information from the ra.xml.  Properties for the ManagedConectionFactory should be supplied with their values in the ManagedConnectionFactoryProperties element.
 *
 *
 * Created: Fri Feb  8 13:44:31 2002
 *
 * @author <a href="toby.allsopp@peace.com">Toby Allsopp</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 * @jmx:mbean name="jboss.jca:service=RARDeployment"
 *            extends="org.jboss.system.ServiceMBean, javax.resource.spi.ManagedConnectionFactory"
 */

public class RARDeployment
   extends ServiceMBeanSupport
   implements RARDeploymentMBean, ManagedConnectionFactory
{

   public final static String MCF_ATTRIBUTE_CHANGED_NOTIFICATION = "jboss.mcfattributechangednotification";

   private Logger log = Logger.getLogger(getClass());

   //Hack to use previous ra.xml parsing code until xslt deployment is written.

   private ObjectName oldRarDeployment;

   private String displayName;
   private String vendorName;
   private String specVersion;
   private String eisType;
   private String version;
   private String managedConnectionFactoryClass;
   private String connectionFactoryInterface;
   private String connectionFactoryImplClass;
   private String connectionInterface;
   private String connectionImplClass;
   private String transactionSupport;
   private Element managedConnectionFactoryProperties;
   private String authenticationMechanismType;
   private String credentialInterface;
   private boolean reauthenticationSupport;

   private Class mcfClass;
   private ManagedConnectionFactory mcf;

   /**
    * Default managed constructor for RARDeployment mbeans.
    *
    * @jmx.managed-constructor
    */
   public RARDeployment ()
   {

   }




   /**
    * The OldRarDeployment attribute refers to a previous-generation RARDeployment.
    * THIS IS A HACK UNTIL XSLT DEPLOYMENT IS WRITTEN
    *
    * @return value of OldRarDeployment
    *
    * @jmx:managed-attribute
    * @todo remove this when xslt based deployment is written.
    */
   public ObjectName getOldRarDeployment()
   {
      return oldRarDeployment;
   }


   /**
    * Set the value of OldRarDeployment
    * @param OldRarDeployment  Value to assign to OldRarDeployment
    *
    * @jmx:managed-attribute
    * @todo remove this when xslt based deployment is written.
    */
   public void setOldRarDeployment(final ObjectName oldRarDeployment)
   {
      this.oldRarDeployment = oldRarDeployment;
   }




   /**
    * The DisplayName attribute holds the DisplayName from the ra.xml
    * It should be supplied by xslt from ra.xml
    *
    * @return the DisplayName value.
    * @jmx:managed-attribute
    */
   public String getDisplayName()
   {
      return displayName;
   }

   /**
    * Set the DisplayName value.
    * @param displayName The new DisplayName value.
    * @jmx:managed-attribute
    */
   public void setDisplayName(String displayName)
   {
      this.displayName = displayName;
   }


   /**
    * The VendorName attribute holds the VendorName from the ra.xml
    * It should be supplied by xslt from ra.xml
    *
    * @return the VendorName value.
    * @jmx:managed-attribute
    */
   public String getVendorName()
   {
      return vendorName;
   }

   /**
    * Set the VendorName value.
    * @param vendorName The new VendorName value.
    * @jmx:managed-attribute
    */
   public void setVendorName(String vendorName)
   {
      this.vendorName = vendorName;
   }


   /**
    * The SpecVersion attribute holds the SpecVersion from the ra.xml
    * It should be supplied by xslt from ra.xml
    *
    * @return the SpecVersion value.
    * @jmx:managed-attribute
    */
   public String getSpecVersion()
   {
      return specVersion;
   }

   /**
    * Set the SpecVersion value.
    * @param specVersion The new SpecVersion value.
    * @jmx:managed-attribute
    */
   public void setSpecVersion(String specVersion)
   {
      this.specVersion = specVersion;
   }


   /**
    * The EisType attribute holds the EisType from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the EisType value.
    * @jmx:managed-attribute
    */
   public String getEisType()
   {
      return eisType;
   }

   /**
    * Set the EisType value.
    * @param eisType The new EisType value.
    * @jmx:managed-attribute
    */
   public void setEisType(String eisType)
   {
      this.eisType = eisType;
   }


   /**
    * The Version attribute holds the Version from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the Version value.
    * @jmx:managed-attribute
    */
   public String getVersion()
   {
      return version;
   }

   /**
    * Set the Version value.
    * @param version The new Version value.
    * @jmx:managed-attribute
    */
   public void setVersion(String version)
   {
      this.version = version;
   }


   /**
    * The ManagedConnectionFactoryClass attribute holds the ManagedConnectionFactoryClass from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the ManagedConnectionFactoryClass value.
    * @jmx:managed-attribute
    */
   public String getManagedConnectionFactoryClass()
   {
      return managedConnectionFactoryClass;
   }

   /**
    * Set the ManagedConnectionFactoryClass value.
    * @param managedConnectionFactoryClass The new ManagedConnectionFactoryClass value.
    * @jmx:managed-attribute
    */
   public void setManagedConnectionFactoryClass(final String managedConnectionFactoryClass)
   {
      this.managedConnectionFactoryClass = managedConnectionFactoryClass;
   }



   /**
    * The ConnectionFactoryInterface attribute holds the ConnectionFactoryInterface from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the ConnectionFactoryInterface value.
    * @jmx:managed-attribute
    */
   public String getConnectionFactoryInterface()
   {
      return connectionFactoryInterface;
   }

   /**
    * Set the ConnectionFactoryInterface value.
    * @param connectionFactoryInterface The ConnectionFactoryInterface value.
    * @jmx:managed-attribute
    */
   public void setConnectionFactoryInterface(String connectionFactoryInterface)
   {
      this.connectionFactoryInterface = connectionFactoryInterface;
   }


   /**
    * The ConnectionFactoryImplClass attribute holds the ConnectionFactoryImplClass from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the ConnectionFactoryImplClass value.
    * @jmx:managed-attribute
    */
   public String getConnectionFactoryImplClass()
   {
      return connectionFactoryImplClass;
   }

   /**
    * Set the ConnectionFactoryImplClass value.
    * @param connectionFactoryImplClass The ConnectionFactoryImplClass value.
    * @jmx:managed-attribute
    */
   public void setConnectionFactoryImplClass(String connectionFactoryImplClass)
   {
      this.connectionFactoryImplClass = connectionFactoryImplClass;
   }


   /**
    * The ConnectionInterface attribute holds the ConnectionInterface from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the ConnectionInterface value.
    * @jmx:managed-attribute
    */
   public String getConnectionInterface()
   {
      return connectionInterface;
   }

   /**
    * Set the ConnectionInterface value.
    * @param connectionInterface The ConnectionInterface value.
    * @jmx:managed-attribute
    */
   public void setConnectionInterface(String connectionInterface)
   {
      this.connectionInterface = connectionInterface;
   }


   /**
    * The ConnectionImplClass attribute holds the ConnectionImplClass from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the connectionImplClass value.
    * @jmx:managed-attribute
    */
   public String getConnectionImplClass()
   {
      return connectionImplClass;
   }

   /**
    * Set the ConnectionImplClass value.
    * @param connectionImplClass The ConnectionImplClass value.
    * @jmx:managed-attribute
    */
   public void setConnectionImplClass(String connectionImplClass)
   {
      this.connectionImplClass = connectionImplClass;
   }


   /**
    * The TransactionSupport attribute holds the TransactionSupport from the ra.xml.
    * It should be supplied by xslt from ra.xml
    * It is ignored, and choice of ConnectionManager implementations determine
    * transaction support.
    *
    * Get the TransactionSupport value.
    * @return the TransactionSupport value.
    * @jmx:managed-attribute
    */
   public String getTransactionSupport()
   {
      return transactionSupport;
   }

   /**
    * Set the TransactionSupport value.
    * @param transactionSupport The TransactionSupport value.
    * @jmx:managed-attribute
    */
   public void setTransactionSupport(String transactionSupport)
   {
      this.transactionSupport = transactionSupport;
   }


   /**
    * The ManagedConnectionFactoryProperties attribute holds the
    * ManagedConnectionFactoryProperties from the ra.xml, together with
    * user supplied values for all or some of these properties.  This must be
    * supplied as an element in the same format as in ra.xml, wrapped in a
    * properties tag.
    * It should be supplied by xslt from ra.xml merged with an user
    * configuration xml file.
    * An alternative format has a config-property element with attributes for
    * name and type and the value as content.
    *
    * @return the ManagedConnectionFactoryProperties value.
    * @jmx:managed-attribute
    */
   public Element getManagedConnectionFactoryProperties()
   {
      return managedConnectionFactoryProperties;
   }

   /**
    * Set the ManagedConnectionFactoryProperties value.
    * @param managedConnectionFactoryProperties The ManagedConnectionFactoryProperties value.
    * @jmx:managed-attribute
    */
   public void setManagedConnectionFactoryProperties(Element managedConnectionFactoryProperties)
   {
      this.managedConnectionFactoryProperties = managedConnectionFactoryProperties;
   }


   /**
    * The AuthenticationMechanismType attribute holds the AuthenticationMechanismType from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the AuthenticationMechanismType value.
    * @jmx:managed-attribute
    */
   public String getAuthenticationMechanismType()
   {
      return authenticationMechanismType;
   }

   /**
    * Set the AuthenticationMechanismType value.
    * @param authenticationMechanismType The AuthenticationMechanismType value.
    * @jmx:managed-attribute
    */
   public void setAuthenticationMechanismType(String authenticationMechanismType)
   {
      this.authenticationMechanismType = authenticationMechanismType;
   }


   /**
    * The CredentialInterface attribute holds the CredentialInterface from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the CredentialInterface value.
    * @jmx:managed-attribute
    */
   public String getCredentialInterface()
   {
      return credentialInterface;
   }

   /**
    * Set the CredentialInterface value.
    * @param credentialInterface The CredentialInterface value.
    * @jmx:managed-attribute
    */
   public void setCredentialInterface(String credentialInterface)
   {
      this.credentialInterface = credentialInterface;
   }


   /**
    * The ReauthenticationSupport attribute holds the ReauthenticationSupport from the ra.xml.
    * It should be supplied by xslt from ra.xml
    *
    * @return the ReauthenticationSupport value.
    * @jmx:managed-attribute
    */
   public boolean isReauthenticationSupport()
   {
      return reauthenticationSupport;
   }

   /**
    * Set the ReauthenticationSupport value.
    * @param reauthenticationSupport The ReauthenticationSupport value.
    * @jmx:managed-attribute
    */
   public void setReauthenticationSupport(boolean reauthenticationSupport)
   {
      this.reauthenticationSupport = reauthenticationSupport;
   }

   /**
    * The <code>getMcfInstance</code> method returns the
    * ManagedConnectionFactory instance represented by this mbean.
    * It is needed so PasswordCredentials can match up correctly.
    * This will probably have to be implemented as an interceptor when
    * the mcf is directly deployed as an mbean.
    *
    * @return a <code>ManagedConnectionFactory</code> value
    *
    * @jmx.managed-attribute access="read-only" description="Returns ManagedConnectionFactory instance represented by this mbean"
    */
   public ManagedConnectionFactory getMcfInstance()
   {
      return mcf;
   }

   /**
    * Describe <code>startManagedConnectionFactory</code> method here.
    * creates managedConnectionFactory, creates ConnectionFactory, and binds it in jndi.
    * Returns the ManagedConnectionFactory to the ConnectionManager that called us.
    *
    * @return a <code>ManagedConnectionFactory</code> value
    * @todo remove use of oldRarDeployment when xslt based deployment is written.
    */
   protected void startService()
      throws Exception
   {
      if (mcf != null)
      {
         throw new DeploymentException("Stop the RARDeployment before restarting it");
      } // end of if ()
      //WARNING HACK
      if (oldRarDeployment != null)
      {
         copyRaInfo();
      } // end of if ()


      try
      {
         mcfClass = Thread.currentThread().getContextClassLoader().loadClass(managedConnectionFactoryClass);
      }
      catch (ClassNotFoundException cnfe)
      {
         log.error("Could not find ManagedConnectionFactory class: " + managedConnectionFactoryClass, cnfe);
         throw new DeploymentException("Could not find ManagedConnectionFactory class: " + managedConnectionFactoryClass);
      } // end of try-catch
      try
      {
         mcf = (ManagedConnectionFactory)mcfClass.newInstance();
      }
      catch (Exception e)
      {
         log.error("Could not instantiate ManagedConnectionFactory: " + managedConnectionFactoryClass, e);
         throw new DeploymentException("Could not instantiate ManagedConnectionFactory: " + managedConnectionFactoryClass);
      } // end of try-catch

      //set default properties;
      if (oldRarDeployment != null)
      {
         setMcfProperties((Element)getServer().getAttribute(oldRarDeployment, "ResourceAdapterElement"));
      }
      //set overridden properties;
      setMcfProperties(managedConnectionFactoryProperties);
   }

   /**
    * The <code>stopManagedConnectionFactory</code> method unbinds the ConnectionFactory
    * from jndi, releases the ManagedConnectionFactory instane, and releases the
    * ManagedConnectionFactory class.
    *
    */
   protected void stopService()
   {
      mcf = null;
      mcfClass = null;
   }

   /**
    * The setManagedConnectionFactoryAttribute method can be used to set
    * attributes on the ManagedConnectionFactory from code, without using the
    * xml configuration.
    *
    * @param name a <code>String</code> value
    * @param clazz a <code>Class</code> value
    * @param value an <code>Object</code> value
    *
    * @jmx:managed-operation
    */
   public void setManagedConnectionFactoryAttribute(String name, Class clazz, Object value)
   {
      Method setter;

      try
      {
         setter = mcfClass.getMethod("set" + name, new Class[]{clazz});
      }
      catch (NoSuchMethodException nsme)
      {
         log.warn("The class '" + mcfClass.toString() + "' has no " +
                  "setter for config property '" + name + "'");
         throw new IllegalArgumentException("The class '" + mcfClass.toString() + "' has no " +
                                            "setter for config property '" + name + "'");
      }
      try
      {
         setter.invoke(mcf, new Object[]{value});
         log.debug("set property " + name + " to value " + value);
      }
      catch (Exception e)
      {
         log.warn("Unable to invoke setter method '" + setter + "' " +
                  "on object '" + mcf + "'", e);
         throw new IllegalArgumentException("Unable to invoke setter method '" + setter + "' " +
                                            "on object '" + mcf + "'");
      }
      sendNotification(new Notification(MCF_ATTRIBUTE_CHANGED_NOTIFICATION, getServiceName(), getNextNotificationSequenceNumber()));
   }

   /**
    * The <code>getManagedConnectionFactoryAttribute</code> method can be used
    * to examine the managed connection factory properties.
    *
    * @param name a <code>String</code> value
    * @return an <code>Object</code> value
    *
    * @jmx:managed-operation
    */
   public Object getManagedConnectionFactoryAttribute(String name)
   {
      Method getter;

      try
      {
         getter = mcfClass.getMethod("get" + name, new Class[]{});
      }
      catch (NoSuchMethodException nsme)
      {
         log.warn("The class '" + mcfClass.toString() + "' has no " +
                  "getter for config property '" + name + "'");
         throw new IllegalArgumentException("The class '" + mcfClass.toString() + "' has no " +
                                            "getter for config property '" + name + "'");
      }
      try
      {
         Object value = getter.invoke(mcf, new Object[]{});
         log.debug("get property " + name + ": value " + value);
         return value;
      }
      catch (Exception e)
      {
         log.warn("Unable to invoke getter method '" + getter + "' " +
                  "on object '" + mcf + "'", e);
         throw new IllegalArgumentException("Unable to invoke getter method '" + getter + "' " +
                                            "on object '" + mcf + "'");
      }
   }


   //ObjectFactory implementation

   //protected methods

   protected void setMcfProperties(Element mcfProps) throws DeploymentException
   {
      if (mcfProps == null)
      {
         return;
      } // end of if ()

      // See if the config has disabled property replacement
      boolean replace = true;
      String replaceAttr = mcfProps.getAttribute("replace");
      if( replaceAttr.length() > 0 )
         replace = Boolean.valueOf(replaceAttr).booleanValue();

      // the properties that the deployment descriptor says we need to set
      NodeList props = mcfProps.getChildNodes();
      for (int i = 0;  i < props.getLength(); i++ )
      {
         if (props.item(i).getNodeType() == Node.ELEMENT_NODE)
         {
            Element prop = (Element)props.item(i);
            if (prop.getTagName().equals("config-property"))
            {
               String name = null;
               String type = null;
               String value = null;
               //Support for more friendly config style
               //<config-property name="" type=""></config-property>
               if (prop.hasAttribute("name"))
               {
                  name = prop.getAttribute("name");
                  type = prop.getAttribute("type");
                  value = MetaData.getElementContent(prop);
               } // end of if ()
               else
               {
                  name = MetaData.getElementContent(
                     MetaData.getUniqueChild(prop, "config-property-name"));
                  type = MetaData.getElementContent(
                     MetaData.getUniqueChild(prop, "config-property-type"));
                  value = MetaData.getElementContent(
                     MetaData.getOptionalChild(prop, "config-property-value"));
               } // end of else
               if (name == null || name.length() == 0 || value == null || value.length() == 0)
               {
                  log.debug("Not setting config property '" + name + "'");
                  continue;
               }
               if (type == null || type.length() == 0)
               {
                  // Default to String for convenience.
                  type = "java.lang.String";
               } // end of if ()

               // see if it is a primitive type first
               Class clazz = Classes.getPrimitiveTypeForName(type);
               if (clazz == null)
               {
                  //not primitive, look for it.
                  try
                  {
                     clazz = Thread.currentThread().getContextClassLoader().loadClass(type);
                  }
                  catch (ClassNotFoundException cnfe)
                  {
                     log.warn("Unable to find class '" + type + "' for " +
                              "property '" + name + "' - skipping property.");
                     continue;
                  }
               }
               PropertyEditor pe = PropertyEditorManager.findEditor(clazz);
               if (pe == null)
               {
                  log.warn("Unable to find a PropertyEditor for class '" +
                           clazz + "' of property '" + name + "' - " +
                           "skipping property");
                  continue;
               }

               if( replace == true )
                  value = Strings.replaceProperties(value);
               log.debug("setting property: " + name + " to value " + value);
               try
               {
                  pe.setAsText(value);
               }
               catch (IllegalArgumentException iae)
               {
                  log.warn("Value '" + value + "' is not valid for property '" +
                           name + "' of class '" + clazz + "' - skipping " +
                           "property");
                  continue;
               }
               Object v = pe.getValue();
               setManagedConnectionFactoryAttribute(name, clazz, v);
            } // end of if ()
         } // end of if ()
      } //end of for
   }

   /**
    * Describe <code>copyRaInfo</code> method here.
    *
    * @exception Exception if an error occurs
    * @todo remove this when xslt based deployment is written.
    */
   private void copyRaInfo() throws DeploymentException
   {
      try
      {
         RARMetaData rarMD = (RARMetaData)getServer().getAttribute(oldRarDeployment, "RARMetaData");
         setDisplayName(rarMD.getDisplayName());
         setManagedConnectionFactoryClass(rarMD.getManagedConnectionFactoryClass());
         //setTransactionSupport(rarMD.getTransactionSupport());
         //set(rarMD.getProperties());//???
         //setAuthMechType(rarMD.getAuthMechType());
         setReauthenticationSupport(rarMD.getReauthenticationSupport());
      }
      catch (Exception e)
      {
         throw new DeploymentException("couldn't get oldRarDeployment!", e);
      } // end of try-catch

   }

   //ManagedConnectionFactory implementation, used to keep backward compatibility
   // between configs using this mbean and newer xmbean based configs.

    /**
     * Creates a connection factory instance.
     */
    public Object createConnectionFactory() throws ResourceException
   {
      return mcf.createConnectionFactory();
   }

    /**
     * Creates a connection factory instance.
     */
    public Object createConnectionFactory( ConnectionManager cxManager )
        throws ResourceException
   {
      return mcf.createConnectionFactory(cxManager);
   }

    /**
     * Creates a new ManagedConnection
     */
    public ManagedConnection createManagedConnection( javax.security.auth.Subject subject, ConnectionRequestInfo cxRequestInfo ) throws ResourceException
   {
      return mcf.createManagedConnection(subject, cxRequestInfo);
   }

    /**
     * Tests object for equality
     */
    public boolean equals( Object other )
   {
      return mcf.equals(other);
   }

    /**
     * Gets the logwriter for this instance.
     */
    public java.io.PrintWriter getLogWriter() throws ResourceException
   {
      return mcf.getLogWriter();
   }


    /**
     * Generates a hashCode for this object
     */
    public int hashCode()
   {
      return mcf.hashCode();
   }

    /**
     * Returns a matching connection from the set.
     */
    public ManagedConnection matchManagedConnections(
                                  java.util.Set connectionSet,
                                  javax.security.auth.Subject subject,
                                  ConnectionRequestInfo cxRequestInfo )
                throws ResourceException
   {
      return mcf.matchManagedConnections(connectionSet, subject, cxRequestInfo);
   }

    /**
     * Sets the logwriter for this instance.
     */
    public void setLogWriter( java.io.PrintWriter out ) throws ResourceException
   {
      mcf.setLogWriter(out);
   }


}// RARDeployment
