/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.metadata;

import java.beans.IntrospectionException;
import java.io.InputStream;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.HashSet;

import javax.management.Descriptor;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.NotCompliantMBeanException;
import javax.management.modelmbean.DescriptorSupport;
import javax.management.modelmbean.ModelMBeanAttributeInfo;
import javax.management.modelmbean.ModelMBeanConstructorInfo;
import javax.management.modelmbean.ModelMBeanInfo;
import javax.management.modelmbean.ModelMBeanInfoSupport;
import javax.management.modelmbean.ModelMBeanNotificationInfo;
import javax.management.modelmbean.ModelMBeanOperationInfo;

import org.jboss.mx.modelmbean.XMBeanConstants;
import org.jboss.logging.Logger;
import org.jboss.util.Strings;
import org.jboss.util.propertyeditor.PropertyEditors;

import org.jdom.Attribute;
import org.jdom.Element;
import org.jdom.JDOMException;
import org.jdom.input.DOMBuilder;
import org.jdom.input.SAXBuilder;

/** The JBoss model mbean descriptor parser class.
 *
 * @author Matt Munz
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.3.4.10 $
 */
public class JBossXMBean10
      extends AbstractBuilder
      implements XMBeanConstants
{
   private static Logger log = Logger.getLogger(JBossXMBean10.class);
   // Attributes ----------------------------------------------------

   /**
    * URL for the XML definition of the management interface
    */
   private URL url = null;
   /** The mbean element from the xml descriptor */
   private org.w3c.dom.Element element;

   /**
    * The class name of the Model MBean implementation class.
    */
   private String mmbClassName = null;

   /**
    * The class name of the resource object represented by this Model MBean.
    */
   private String resourceClassName = null;


   // Constructors --------------------------------------------------

   /**
    * Initialized a parser for the JBossMX 1.0 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    */
   public JBossXMBean10(String mmbClassName, String resourceClassName, URL url)
   {
      super();

      this.url = url;
      this.mmbClassName = mmbClassName;
      this.resourceClassName = resourceClassName;
   }

   /**
    * Initialized a parser for the JBossMX 1.0 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    *
    * @throws MalformedURLException if the given management interface URL cannot
    *         be resolved
    */
   public JBossXMBean10(String mmbClassName, String resourceClassName, String url)
      throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url));
   }

   /**
    * Initialized a parser for the JBossMX 1.0 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    */
   public JBossXMBean10(String mmbClassName, String resourceClassName, URL url, Map properties)
   {
      this(mmbClassName, resourceClassName, url);
      setProperties(properties);
   }

   /**
    * Initialized a parser for the JBossMX 1.0 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    *
    * @throws MalformedURLException if the given management interface URL cannot
    *         be resolved
    */
   public JBossXMBean10(String mmbClassName, String resourceClassName,
         String url, Map properties) throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url), properties);
   }

   public JBossXMBean10(String mmbClassName, String resourceClassName,
      org.w3c.dom.Element element)
   {
      this.mmbClassName = mmbClassName;
      this.resourceClassName = resourceClassName;
      this.element = element;
   }

   // MetaDataBuilder implementation --------------------------------

   public MBeanInfo build() throws NotCompliantMBeanException
   {
      try
      {
         Element root = null;
         if (element == null)
         {
            // by default, let JAXP pick the SAX parser
            SAXBuilder builder = null;

            // check if user wants to override the SAX parser property
            if (properties.get(SAX_PARSER) != null)
            {
               builder = new SAXBuilder(getStringProperty(SAX_PARSER));
            }
            else
            {
               builder = new SAXBuilder();
            }

            // by default we validate
            builder.setValidation(true);

            // the user can override the validation by setting the VALIDATE property
            try
            {
               boolean validate = getBooleanProperty(XML_VALIDATION);
               builder.setValidation(validate);
            }
            catch (IllegalPropertyException e)
            {
               // fall through, use the default value
            }

            //supply it with our dtd locally.
            builder.setEntityResolver(new XMBeanEntityResolver());

            // get the root and start parsing...
            InputStream docStream = url.openStream();
            root = builder.build(docStream).getRootElement();
            docStream.close();
         }
         else
         {
            DOMBuilder builder = new DOMBuilder();
            root = builder.build(element);
         }

         String description = root.getChildText("description");

         if (resourceClassName == null)
         {
            resourceClassName = root.getChildText("class");
         }

         List constructors = root.getChildren("constructor");
         List operations = root.getChildren("operation");
         List attributes = root.getChildren("attribute");
         List notifications = root.getChildren("notifications");

         Descriptor descr = getDescriptor(root, resourceClassName, "mbean");

         ModelMBeanInfo info = buildMBeanMetaData(
               description, constructors, operations,
               attributes, notifications, descr
         );

         return (MBeanInfo) info;
      }
      catch (JDOMException e)
      {
         Throwable cause = e.getCause();
         String msg = "DOM exception while parsing url:" + url
            + " cause: "+ (cause == null ? e.toString() : cause.toString());
         // Log the exception to generate complete stack trace
         log.debug(msg, e);
         throw new NotCompliantMBeanException(msg);
      }
      catch (IOException e)
      {
         String msg = "IO Error parsing the XML file: " + url
            + " cause: " + e.toString();
         log.debug(msg, e);
         throw new NotCompliantMBeanException(msg);
      }
   }

   // Protected -----------------------------------------------------

   protected Descriptor getDescriptor(final Element parent, final String infoName,
      final String type) throws NotCompliantMBeanException
   {
      Descriptor descr = new DescriptorSupport();
      descr.setField(NAME, infoName);
      descr.setField(DESCRIPTOR_TYPE, type);

      Element descriptors = parent.getChild("descriptors");
      if (descriptors == null)
      {
         return descr;
      } // end of if ()
      for (Iterator i = descriptors.getChildren().iterator(); i.hasNext();)
      {
         Element descriptor = (Element) i.next();
         String name = descriptor.getName();
         if (name.equals("persistence"))
         {
            Attribute persistPolicy = descriptor.getAttribute(PERSIST_POLICY);
            Attribute persistPeriod = descriptor.getAttribute(PERSIST_PERIOD);
            Attribute persistLocation = descriptor.getAttribute(PERSIST_LOCATION);
            Attribute persistName = descriptor.getAttribute(PERSIST_NAME);
            if (persistPolicy != null)
            {
               String value = persistPolicy.getValue();
               validate(value, PERSIST_POLICY_LIST);
               descr.setField(PERSIST_POLICY, value);
            }
            if (persistPeriod != null)
            {
               descr.setField(PERSIST_PERIOD, persistPeriod.getValue());
            }
            if (persistLocation != null)
            {
               descr.setField(PERSIST_LOCATION, persistLocation.getValue());
            }
            if (persistName != null)
            {
               descr.setField(PERSIST_NAME, persistName.getValue());
            }
         }
         else if (name.equals(CURRENCY_TIME_LIMIT))
         {
            descr.setField(CURRENCY_TIME_LIMIT, descriptor.getAttributeValue("value"));
         } // end of else
         else if (name.equals(STATE_ACTION_ON_UPDATE))
         {
            String value = descriptor.getAttributeValue("value");
            validate(value, STATE_ACTION_ON_UPDATE_LIST);
            descr.setField(STATE_ACTION_ON_UPDATE, value);
         } // end of else
         else if (name.equals(DEFAULT))
         {
            String value = descriptor.getAttributeValue("value");
            descr.setField(DEFAULT, value);
         }
         else if (name.equals(VALUE))
         {
            String value = descriptor.getAttributeValue("value");
            descr.setField(VALUE, value);
         }
         else if (name.equals(PERSISTENCE_MANAGER))
         {
            descr.setField(PERSISTENCE_MANAGER, descriptor.getAttributeValue("value"));
         }
         else if (name.equals(DESCRIPTOR))
         {
            descr.setField(descriptor.getAttributeValue("name"), descriptor.getAttributeValue("value"));
         }
         else if(name.equals(INTERCEPTORS))
         {
            Descriptor[] interceptorDescriptors = buildInterceptors(descriptor);
            descr.setField(INTERCEPTORS, interceptorDescriptors);
         } // end of else
      } // end of for ()
      return descr;
   }

   private void validate(String value, String[] valid) throws NotCompliantMBeanException
   {
      for (int i = 0; i < valid.length; i++)
      {
         if (valid[i].equalsIgnoreCase(value))
         {
            return;
         } // end of if ()
      } // end of for ()
      throw new NotCompliantMBeanException("Unknown descriptor value: " + value);
   }


   // builder methods

   protected ModelMBeanInfo buildMBeanMetaData(String description,
         List constructors, List operations, List attributes,
         List notifications, Descriptor descr)
         throws NotCompliantMBeanException
   {

      ModelMBeanOperationInfo[] operInfo =
            buildOperationInfo(operations, attributes);
      ModelMBeanAttributeInfo[] attrInfo =
            buildAttributeInfo(attributes);
      ModelMBeanConstructorInfo[] constrInfo =
            buildConstructorInfo(constructors);
      ModelMBeanNotificationInfo[] notifInfo =
            buildNotificationInfo(notifications);

      ModelMBeanInfo info = new ModelMBeanInfoSupport(
            mmbClassName, description, attrInfo, constrInfo,
            operInfo, notifInfo, descr
      );

      return info;
   }


   protected ModelMBeanConstructorInfo[] buildConstructorInfo(List constructors)
         throws NotCompliantMBeanException
   {

      List infos = new ArrayList();

      for (Iterator it = constructors.iterator(); it.hasNext();)
      {
         Element constr = (Element) it.next();
         String name = constr.getChildTextTrim("name");
         String description = constr.getChildTextTrim("description");
         List params = constr.getChildren("parameter");

         MBeanParameterInfo[] paramInfo =
               buildParameterInfo(params);

         Descriptor descr = getDescriptor(constr, name, CONSTRUCTOR_DESCRIPTOR);

         ModelMBeanConstructorInfo info =
               new ModelMBeanConstructorInfo(name, description, paramInfo, descr);

         infos.add(info);
      }

      return (ModelMBeanConstructorInfo[]) infos.toArray(
            new ModelMBeanConstructorInfo[0]);
   }

   protected ModelMBeanOperationInfo[] buildOperationInfo(List operations, List attributes)
         throws NotCompliantMBeanException
   {
      List infos = new ArrayList();

      // Map of method names to types for possible getters
      HashMap getters = new HashMap();

      // Map of method names to a set of types for possible setters
      HashMap setters = new HashMap();

      for (Iterator it = operations.iterator(); it.hasNext();)
      {
         Element oper = (Element) it.next();
         String name = oper.getChildTextTrim("name");
         String description = oper.getChildTextTrim("description");
         String type = oper.getChildTextTrim("return-type");
         String impact = oper.getAttributeValue("impact");
         List params = oper.getChildren("parameter");

         MBeanParameterInfo[] paramInfo =
               buildParameterInfo(params);

         Descriptor descr = getDescriptor(oper, name, OPERATION_DESCRIPTOR);

         // defaults to ACTION_INFO
         int operImpact = MBeanOperationInfo.ACTION_INFO;

         if (impact != null)
         {
            if (impact.equals(INFO))
               operImpact = MBeanOperationInfo.INFO;
            else if (impact.equals(ACTION))
               operImpact = MBeanOperationInfo.ACTION;
            else if (impact.equals(ACTION_INFO))
               operImpact = MBeanOperationInfo.ACTION_INFO;
         }

         // default return-type is void
         if (type == null)
            type = "void";

         // Possible getter?
         if (paramInfo.length == 0 && type.equals("void") == false)
            getters.put(name, type);

         // Possible setter?
         if (paramInfo.length == 1)
         {
            HashSet types = (HashSet) setters.get(name);
            if (types == null)
            {
               types = new HashSet();
               setters.put(name, types);
            }
            types.add(paramInfo[0].getType());
         }

         ModelMBeanOperationInfo info = new ModelMBeanOperationInfo(
               name, description, paramInfo, type, operImpact, descr);

         infos.add(info);
      }

      // Add operations for get/setMethod that aren't already present

      for (Iterator it = attributes.iterator(); it.hasNext();)
      {
         Element attr = (Element) it.next();
         String name = attr.getChildTextTrim("name");
         String type = attr.getChildTextTrim("type");
         String getMethod = attr.getAttributeValue(GET_METHOD);
         String setMethod = attr.getAttributeValue(SET_METHOD);

         // Fabricate a getter operation
         if (getMethod != null)
         {
            Object getterOpType = getters.get(getMethod);
            if (getterOpType == null || getterOpType.equals(type) == false)
            {
               Descriptor getterDescriptor = new DescriptorSupport();
               getterDescriptor.setField(NAME, getMethod);
               getterDescriptor.setField(DESCRIPTOR_TYPE, OPERATION_DESCRIPTOR);
               getterDescriptor.setField(ROLE, GETTER);
               ModelMBeanOperationInfo info = new ModelMBeanOperationInfo
                     (
                           getMethod,
                           "getMethod operation for '" + name + "' attribute.",
                           new MBeanParameterInfo[0],
                           type,
                           MBeanOperationInfo.INFO,
                           getterDescriptor
                     );
               infos.add(info);
            }
         }

         // Fabricate a setter operation
         if (setMethod != null)
         {
            HashSet setterOpTypes = (HashSet) setters.get(setMethod);
            if (setterOpTypes == null || setterOpTypes.contains(type) == false)
            {
               Descriptor setterDescriptor = new DescriptorSupport();
               setterDescriptor.setField(NAME, setMethod);
               setterDescriptor.setField(DESCRIPTOR_TYPE, OPERATION_DESCRIPTOR);
               setterDescriptor.setField(ROLE, SETTER);
               ModelMBeanOperationInfo info = new ModelMBeanOperationInfo
                     (
                           setMethod,
                           "setMethod operation for '" + name + "' attribute.",
                           new MBeanParameterInfo[]
                           {
                              new MBeanParameterInfo("value", type, "The new value")
                           },
                           Void.TYPE.getName(),
                           MBeanOperationInfo.ACTION,
                           setterDescriptor
                     );
               infos.add(info);
            }
         }
      }

      return (ModelMBeanOperationInfo[]) infos.toArray(
            new ModelMBeanOperationInfo[0]);
   }


   protected ModelMBeanNotificationInfo[] buildNotificationInfo(List notifications)
         throws NotCompliantMBeanException
   {

      List infos = new ArrayList();

      for (Iterator it = notifications.iterator(); it.hasNext();)
      {
         Element notif = (Element) it.next();
         String name = notif.getChildTextTrim("name");
         String description = notif.getChildTextTrim("description");
         List notifTypes = notif.getChildren("notification-type");
         Descriptor descr = getDescriptor(notif, name, NOTIFICATION_DESCRIPTOR);

         List types = new ArrayList();

         for (Iterator iterator = notifTypes.iterator(); iterator.hasNext();)
         {
            Element type = (Element) iterator.next();
            types.add(type.getTextTrim());
         }

         ModelMBeanNotificationInfo info = new ModelMBeanNotificationInfo(
               (String[]) types.toArray(), name, description, descr);

         infos.add(info);
      }

      return (ModelMBeanNotificationInfo[]) infos.toArray(
            new ModelMBeanNotificationInfo[0]
      );
   }

   protected ModelMBeanAttributeInfo[] buildAttributeInfo(List attributes)
         throws NotCompliantMBeanException
   {

      List infos = new ArrayList();

      for (Iterator it = attributes.iterator(); it.hasNext();)
      {
         Element attr = (Element) it.next();
         String name = attr.getChildTextTrim("name");
         String description = attr.getChildTextTrim("description");
         String type = attr.getChildTextTrim("type");
         String access = attr.getAttributeValue("access");
         String getMethod = attr.getAttributeValue("getMethod");
         String setMethod = attr.getAttributeValue("setMethod");
         Descriptor descr = getDescriptor(attr, name, ATTRIBUTE_DESCRIPTOR);
         //Convert types here from string to specified type
         String unconvertedValue = (String) descr.getFieldValue(VALUE);
         if (unconvertedValue != null && !"java.lang.String".equals(type))
         {
            descr.setField(VALUE, convertValue(unconvertedValue, type));
         }
         String unconvertedDefault = (String) descr.getFieldValue(DEFAULT);
         if (unconvertedDefault != null && !"java.lang.String".equals(type))
         {
            descr.setField(DEFAULT, convertValue(unconvertedDefault, type));
         }

         if (getMethod != null)
         {
            descr.setField(GET_METHOD, getMethod);
         } // end of if ()
         if (setMethod != null)
         {
            descr.setField(SET_METHOD, setMethod);
         } // end of if ()
         // Check that there is a currencyTimeLimit=-1 if there are no accessors
         if( getMethod == null && setMethod == null )
         {
            Object timeLimit = descr.getFieldValue(CURRENCY_TIME_LIMIT);
            if( timeLimit == null )
               descr.setField(CURRENCY_TIME_LIMIT, "-1");
         }

         // defaults read-write
         boolean isReadable = true;
         boolean isWritable = true;

         if (access.equalsIgnoreCase("read-only"))
            isWritable = false;

         else if (access.equalsIgnoreCase("write-only"))
            isReadable = false;


         ModelMBeanAttributeInfo info = new ModelMBeanAttributeInfo(
               name, type, description, isReadable, isWritable, false, descr
         );


         infos.add(info);
      }

      return (ModelMBeanAttributeInfo[]) infos.toArray(
            new ModelMBeanAttributeInfo[0]
      );
   }

   /** Convert a text value into the corresponding value for typeName using
    * the associated PropertyEditor.
    *
    * @param unconverted a <code>String</code> value
    * @param typeName a <code>String</code> value
    * @return an <code>Object</code> value
    * @exception NotCompliantMBeanException if an error occurs
    */
   protected Object convertValue(String unconverted, String typeName)
         throws NotCompliantMBeanException
   {
      Object value = null;
      try
      {
         value = PropertyEditors.convertValue(unconverted, typeName);
      }
      catch (ClassNotFoundException e)
      {
         log.debug("Failed to load type class", e);
         throw new NotCompliantMBeanException
               ("Class not found for type: " + typeName);
      }
      catch(IntrospectionException e)
      {
         throw new NotCompliantMBeanException
               ("No property editor for type=" + typeName);
      }
      return value;
   }

   protected MBeanParameterInfo[] buildParameterInfo(List parameters)
   {

      Iterator it = parameters.iterator();
      List infos = new ArrayList();

      while (it.hasNext())
      {
         Element param = (Element) it.next();
         String name = param.getChildTextTrim("name");
         String type = param.getChildTextTrim("type");
         String descr = param.getChildTextTrim("description");

         MBeanParameterInfo info = new MBeanParameterInfo(name, type, descr);

         infos.add(info);
      }

      return (MBeanParameterInfo[]) infos.toArray(new MBeanParameterInfo[0]);
   }

   protected Descriptor[] buildInterceptors(Element descriptor)
   {
      List interceptors = descriptor.getChildren("interceptor");
      ArrayList tmp = new ArrayList();
      for(int i = 0; i < interceptors.size(); i ++)
      {
         Element interceptor = (Element) interceptors.get(i);
         String code = interceptor.getAttributeValue("code");
         DescriptorSupport interceptorDescr = new DescriptorSupport();
         interceptorDescr.setField("code", code);
         List attributes = interceptor.getAttributes();
         for(int a = 0; a < attributes.size(); a ++)
         {
            Attribute attr = (Attribute) attributes.get(a);
            String value = attr.getValue();
            value = Strings.replaceProperties(value);
            interceptorDescr.setField(attr.getName(), value);
         }
         tmp.add(interceptorDescr);
      }
      Descriptor[] descriptors = new Descriptor[tmp.size()];
      tmp.toArray(descriptors);
      return descriptors;
   }

   //helper stuff
   //from util.Classes


   /** Primitive type name -> class map. */
   private static final Map PRIMITIVE_NAME_TYPE_MAP = new HashMap();

   /** Setup the primitives map. */
   static
   {
      PRIMITIVE_NAME_TYPE_MAP.put("boolean", Boolean.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("byte", Byte.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("char", Character.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("short", Short.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("int", Integer.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("long", Long.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("float", Float.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("double", Double.TYPE);
   }

   /**
    * Get the primitive type for the given primitive name.
    *
    * <p>
    * For example, "boolean" returns Boolean.TYPE and so on...
    *
    * @param name    Primitive type name (boolean, int, byte, ...)
    * @return        Primitive type or null.
    *
    * @exception IllegalArgumentException    Type is not a primitive class
    */
   public static Class getPrimitiveTypeForName(final String name)
   {
      return (Class) PRIMITIVE_NAME_TYPE_MAP.get(name);
   }

}
