/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.resource;

import java.lang.reflect.Constructor;
import java.util.Iterator;
import java.util.Map;
import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.DynamicMBean;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.ReflectionException;
import org.w3c.dom.Element;

/**
 * RARDeployment.java
 *
 *
 * Created: Fri Sep 28 11:59:19 2001
 *
 * @author <a href="mailto:davidjencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class RARDeployment  implements DynamicMBean 
{

   private final RARMetaData rarMetaData;
   private final MBeanInfo mbi;


   /**
    * Creates a new <code>RARDeployment</code> instance.
    *
    * @param rarMetaData a <code>RARMetaData</code> value
    */
   public RARDeployment (RARMetaData rarMetaData)
   {
      this.rarMetaData = rarMetaData;
      mbi = setupMBeanInfo();
   }
   // implementation of javax.management.DynamicMBean interface

   /**
    * The <code>invoke</code> method throws an MBeanException for RARDeployement
    * because the mbean is read-only.
    *
    * @param operation a <code>String</code> value
    * @param params an <code>Object[]</code> value
    * @param sig a <code>String[]</code> value
    * @return <description>
    * @exception MBeanException always thrown
    * @exception ReflectionException if an error occurs
    */
   public Object invoke(String operation, Object[] params, String[] sig) 
      throws MBeanException
   {
      throw new MBeanException(null, "No operations exposed for RARDeployment");
   }

   /**
    * Describe <code>getAttributes</code> method here.
    *
    * @param AttributeNames a <code>String[]</code> value
    * @return an <code>AttributeList</code> value
    */
   public AttributeList getAttributes(String[] attributeNames) 
   {
      AttributeList atts = new AttributeList();
      for (int i = 0; i < attributeNames.length; i++) 
      {
         try 
         {
            Object value = getAttribute(attributeNames[i]);     
            atts.add(new Attribute(attributeNames[i], value));
         } 
         catch (Exception e) 
         {
         }
      }
      return atts;
   }

   public Object getAttribute(String attributeName) throws AttributeNotFoundException, MBeanException, ReflectionException 
   {
      if (attributeName == null) 
      {
         throw new MBeanException(new AttributeNotFoundException("Null is not an attribute name"), "No attribute name given to getAttribute");
      }
      else if (attributeName.equals("DisplayName")) 
      {
         return rarMetaData.getDisplayName();      
      }
      else if (attributeName.equals("ManagedConnectionFactoryClass")) 
      {
         return rarMetaData.getManagedConnectionFactoryClass();      
      }
      else if (attributeName.equals("TransactionSupport")) 
      {
         return new Integer(rarMetaData.getTransactionSupport());      
      }
      else if (attributeName.equals("AuthMechType")) 
      {
         return rarMetaData.getAuthMechType();      
      }
      else if (attributeName.equals("ReauthenticationSupport")) 
      {
         return new Boolean(rarMetaData.getReauthenticationSupport());      
      }
      else if (attributeName.equals("RARMetaData")) 
      {
         return rarMetaData;
      }
      else if (attributeName.equals("ResourceAdapterElement")) 
      {
         return rarMetaData.getResourceAdapterElement();
      }
      else
      {
         RARMetaData.Property prop = (RARMetaData.Property)rarMetaData.getProperties().get(attributeName);
         if (prop != null) 
         {
            return prop.value;   
         }
      }

      throw new AttributeNotFoundException("No such attribute: " + attributeName);
   }

   public MBeanInfo getMBeanInfo() 
   {
      return mbi;
   }

   public void setAttribute(Attribute param1) throws AttributeNotFoundException
   {
      throw new AttributeNotFoundException("No settable attributes in RARDeployment");
   }

   public AttributeList setAttributes(AttributeList param1) 
   {
      return null;
   }

   private MBeanInfo setupMBeanInfo()
   {
      Map rarProps = rarMetaData.getProperties();
      int attributeCount = 6 + rarProps.size();
      MBeanAttributeInfo[] attributeInfos = new MBeanAttributeInfo[attributeCount];
      MBeanConstructorInfo[] constructorInfos = new MBeanConstructorInfo[1];
      int i = 0;
      attributeInfos[i++] = new MBeanAttributeInfo("DisplayName",
                                              "java.lang.String",
                                              "Display name of the Resource Adapter",
                                              true,
                                              false,
                                              false);
      attributeInfos[i++] = new MBeanAttributeInfo("ManagedConnectionFactoryClass",
                                              "java.lang.String",
                                              "Class name of the ManagedConnectionFactory for this Resource Adapter",
                                              true,
                                              false,
                                              false);
      attributeInfos[i++] = new MBeanAttributeInfo("TransactionSupport",
                                              "java.lang.Integer",
                                              "Transaction support of the Resource Adapter, expressed as a TX_SUPPORT_* constant",
                                              true,
                                              false,
                                              false);
      attributeInfos[i++] = new MBeanAttributeInfo("AuthMechType",
                                              "java.lang.String",
                                              "Authorization mechanism type of the Resource Adapter",
                                              true,
                                              false,
                                              false);
      attributeInfos[i++] = new MBeanAttributeInfo("ReauthenticationSupport",
                                              "java.lang.Boolean",
                                              "Whether this Resource Adapter supports reauthentication of existing connections.",
                                              true,
                                              false,
                                              false);
      attributeInfos[i++] = new MBeanAttributeInfo("ResourceAdapterElement",
                                              "org.w3c.Element",
                                              "ra.xml root element.",
                                              true,
                                              false,
                                              false);
      Iterator propIterator = rarProps.values().iterator();
      while (propIterator.hasNext()) 
      {
         RARMetaData.Property prop = (RARMetaData.Property)propIterator.next();
         attributeInfos[i++] = new MBeanAttributeInfo(prop.name,
                                              prop.type,
                                              prop.desc,
                                              true,
                                              false,
                                              false);

      }

      Constructor[] constructors = getClass().getConstructors();
      constructorInfos[0] = new MBeanConstructorInfo("Constructor for the RARDeployment",
                                                     constructors[0]);
      MBeanInfo mbi = new MBeanInfo(getClass().getName(),
                          "Description of a deployed Resource Adapter",
                          attributeInfos,
                          constructorInfos,
                          new MBeanOperationInfo[0],
                          new MBeanNotificationInfo[0]);
      return mbi;
   }
   
}// Rardeployment
