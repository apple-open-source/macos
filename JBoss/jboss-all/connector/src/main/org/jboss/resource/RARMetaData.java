/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.resource;

import java.lang.StringBuffer;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.XmlLoadable;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

/**
 *  Represents the metadata present in a resource adapter deployment descriptor.
 *
 * @author     Toby Allsopp (toby.allsopp@peace.com)
 * @see        RARDeployer
 * @version    $Revision: 1.14.2.1 $
 */
public class RARMetaData
       implements XmlLoadable
{
   public final static int TX_SUPPORT_NO = 0;
   public final static int TX_SUPPORT_LOCAL = 1;
   public final static int TX_SUPPORT_XA = 2;
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   private Logger category = Logger.getLogger(RARMetaData.class);

   private Element resourceAdapterElement;

   private ClassLoader classLoader;

   private String displayName;

   private String managedConnectionFactoryClass;
   private String connectionFactoryInterface;
   private String connectionFactoryImplClass;
   private String connectionInterface;
   private String connectionImplClass;

   private int transactionSupport;

   private Map properties = new HashMap();

   private String authMechType;
   private String credentialInterface;

   private boolean reauthenticationSupport;

   /**
    *  Sets the ClassLoader attribute of the RARMetaData object
    *
    * @param  cl  The new ClassLoader value
    */
   public void setClassLoader(ClassLoader cl)
   {
      classLoader = cl;
   }

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   public Element getResourceAdapterElement()
   {
      return resourceAdapterElement;
   }

   /**
    *  The class loader to use for the resource adapter's classes
    *
    * @return    The ClassLoader value
    */
   public ClassLoader getClassLoader()
   {
      return classLoader;
   }

   /**
    *  Gets the DisplayName attribute of the RARMetaData object
    *
    * @return    The DisplayName value
    */
   public String getDisplayName()
   {
      return displayName;
   }

   /**
    *  Gets the ManagedConnectionFactoryClass attribute of the RARMetaData
    *  object
    *
    * @return    The ManagedConnectionFactoryClass value
    */
   public String getManagedConnectionFactoryClass()
   {
      return managedConnectionFactoryClass;
   }

   /**
    *  Gets the type of transactions supported by the resource adapter.
    *
    * @return    one of the <code>TX_SUPPORT_*</code> constants
    */
   public int getTransactionSupport()
   {
      return transactionSupport;
   }

   /**
    *  Gets the Properties attribute of the RARMetaData object
    *
    * @return    The Properties value
    */
   public Map getProperties()
   {
      return properties;
   }

   /**
    *  Gets the PropertyType attribute of the RARMetaData object
    *
    * @param  name  Description of Parameter
    * @return       The PropertyType value
    */
   public String getPropertyType(String name)
   {
      Property prop = (Property)properties.get(name);
      return (prop == null) ? null : prop.type;
   }

   /**
    *  Gets the AuthMechType attribute of the RARMetaData object
    *
    * @return    The AuthMechType value
    */
   public String getAuthMechType()
   {
      return authMechType;
   }

   /**
    *  Gets the ReauthenticationSupport attribute of the RARMetaData object
    *
    * @return    The ReauthenticationSupport value
    */
   public boolean getReauthenticationSupport()
   {
      return reauthenticationSupport;
   }

   // XmlLoadable implementation ------------------------------------

   /**
    *  #Description of the Method
    *
    * @param  root                     Description of Parameter
    * @exception  DeploymentException  Description of Exception
    */
   public void importXml(Element root)
          throws DeploymentException
   {

      // First, a quick sanity check to ensure that we're looking at
      // the right kind of deployment descriptor
      String rootTag = root.getTagName();
      if (!rootTag.equals("connector"))
      {
         throw new DeploymentException("Not a resource adapter deployment " +
               "descriptor because its root tag, '" +
               rootTag + "', is not 'connector'");
      }

      // Then, we iterate over all the elements seeing if we're
      // interested

      invokeChildren(root);
   }

   // Setter methods for XML elements

   private void setDisplayName(Element element)
          throws DeploymentException
   {
      displayName = getElementContent(element);
   }

   private void setVendorName(Element element)
   {
   }

   private void setSpecVersion(Element element)
   {
   }

   private void setVersion(Element element)
   {
   }

   private void setEisType(Element element)
   {
   }

   private void setResourceadapter(Element element)
          throws DeploymentException
   {
      resourceAdapterElement = element;
      invokeChildren(element);
   }

   private void setManagedconnectionfactoryClass(Element element)
          throws DeploymentException
   {
      managedConnectionFactoryClass = getElementContent(element);
   }

   private void setConnectionfactoryInterface(Element element)
          throws DeploymentException
   {
      connectionFactoryInterface = getElementContent(element);
   }

   private void setConnectionfactoryImplClass(Element element)
          throws DeploymentException
   {
      connectionFactoryImplClass = getElementContent(element);
   }

   private void setConnectionInterface(Element element)
          throws DeploymentException
   {
      connectionInterface = getElementContent(element);
   }

   private void setConnectionImplClass(Element element)
          throws DeploymentException
   {
      connectionImplClass = getElementContent(element);
   }

   private void setTransactionSupport(Element element)
          throws DeploymentException
   {
      String s = getElementContent(element);
      int ts;
      if (s.equals("NoTransaction"))
      {
         ts = TX_SUPPORT_NO;
      }
      else if (s.equals("LocalTransaction"))
      {
         ts = TX_SUPPORT_LOCAL;
      }
      else if (s.equals("XATransaction"))
      {
         ts = TX_SUPPORT_XA;
      }
      else
      {
         throw new DeploymentException("Invalid transaction support '" +
               s + "', it must be one of " +
               "'NoTransaction', " +
               "'LocalTransaction' or " +
               "'XATransaction'");
      }
      transactionSupport = ts;
   }

   private void setConfigProperty(Element element)
          throws DeploymentException
   {
      Element nameE = MetaData.getUniqueChild(element,
            "config-property-name");
      Element typeE = MetaData.getUniqueChild(element,
            "config-property-type");
      Element valueE = MetaData.getOptionalChild(element,
            "config-property-value");
      Element descE = MetaData.getOptionalChild(element,
            "description");

      Property p = new Property();
      p.name = getElementContent(nameE);
      p.type = getElementContent(typeE);
      if (valueE != null)
      {
         p.value = getElementContent(valueE);
      }
      if (descE != null) 
      {
         p.desc = getElementContent(descE);      
      }

      properties.put(p.name, p);
   }

   private void setLicense(Element element)
          throws DeploymentException
   {
      Element requiredE = MetaData.getUniqueChild(element,
            "license-required");
      Element descriptionE = MetaData.getOptionalChild(element,
            "description");
      boolean required =
            new Boolean(getElementContent(requiredE).trim()).booleanValue();
      if (required)
      {
         category.info("Required license terms present. See deployment " +
               "descriptor.");
      }
      else
      {
         category.info("License terms present. See deployment descriptor.");
      }
   }

   private void setDescription(Element element)
          throws DeploymentException
   {
      category.info("Loading " + getElementContent(element).trim());
   }

   private void setAuthenticationMechanism(Element element)
          throws DeploymentException
   {
      invokeChildren(element);
   }

   private void setAuthenticationMechanismType(Element element)
          throws DeploymentException
   {
      authMechType = getElementContent(element);
   }

   private void setCredentialInterface(Element element)
          throws DeploymentException
   {
      credentialInterface = getElementContent(element);
   }

   private void setReauthenticationSupport(Element element)
          throws DeploymentException
   {
      String value = getElementContent(element);
      if (!value.equals("true") && !value.equals("false"))
      {
         throw new DeploymentException("reauthentication-support must be one " +
               "of 'true' or 'false', not '" + value +
               "'");
      }
      reauthenticationSupport = Boolean.valueOf(value).booleanValue();
   }

   private void setSecurityPermission(Element element)
      throws DeploymentException
   {
      Element permSpec = MetaData.getUniqueChild(element,
         "security-permission-spec");
      String spec = getElementContent(permSpec);
      if (category.isDebugEnabled())
         category.debug("Ignoring security-permission("+spec+")");
   }

   private String getElementContent(Element element)
          throws DeploymentException
   {
      return MetaData.getElementContent(element);
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   private void invokeChildren(Element element)
          throws DeploymentException
   {
      NodeList children = element.getChildNodes();
      for (int i = 0; i < children.getLength(); ++i)
      {
         Node node = children.item(i);
         if (node.getNodeType() == Node.ELEMENT_NODE)
         {
            Element child = (Element)node;
            Method method = elementToMethod(child);
            if (method == null)
            {
               // We don't handle this element. Hope it wasn't
               // important.
               category.warn("Element '" + child + "' not recognised.");
               continue;
            }

            try
            {
               method.invoke(this, new Object[]{child});
            }
            catch (InvocationTargetException ite)
            {
               Throwable t = ite.getTargetException();
               if (t instanceof DeploymentException)
               {
                  throw (DeploymentException)t;
               }
               if (t instanceof Exception)
               {
                  throw new DeploymentException("Exception handling element",
                        (Exception)t);
               }
               if (t instanceof Error)
               {
                  throw (Error)t;
               }
               throw new DeploymentException("WTF?: " + t.toString());
            }
            catch (Exception e)
            {
               throw new DeploymentException("Exception handling element", e);
            }
         }
      }
   }

   private Method elementToMethod(Element element)
   {
      String tag = element.getTagName();

      StringBuffer methodName = new StringBuffer("set");
      {
         // We can't be having hyphens in our method names
         //FIXME this should weed out illegal characters in general

         int last_hyphen = -1;
         int next_hyphen;
         while ((next_hyphen = tag.indexOf('-', last_hyphen + 1)) != -1)
         {
            String thisbit = tag.substring(last_hyphen + 1, next_hyphen);
            methodName.append(toTitleCase(thisbit));
            last_hyphen = next_hyphen;
         }
         methodName.append(toTitleCase(tag.substring(last_hyphen + 1)));
      }

      if (category.isDebugEnabled())
         category.debug("methodName = '" + methodName + "'");

      try
      {
         return getClass().getDeclaredMethod(methodName.toString(),
               new Class[]{Element.class});
      }
      catch (NoSuchMethodException nsme)
      {
         return null;
      }
   }

   private String toTitleCase(String s)
   {
      if (s == null)
      {
         return null;
      }
      if (s.length() == 0)
      {
         return s.toUpperCase();
      }
      return s.substring(0, 1).toUpperCase() + s.substring(1);
   }

   // Inner classes -------------------------------------------------

   /**
    * 
    */
   public class Property
   {
      public String name;
      public String type;
      public String value;
      public String desc;
   }
}
