/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.metadata;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

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
import org.jdom.Attribute;
import org.jdom.Element;
import org.jdom.JDOMException;
import org.jdom.input.SAXBuilder;


/**
 * Parser for the XMBean schema defined in the
 * <a href="http://www.amazon.com/exec/obidos/ASIN/0672322889/002-8100474-1804019">
 * JMX: Managing J2EE with Java Management Extensions</a> (xmbean.dtd)
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3.4.2 $
 */ 
public class XMBeanMetaData
   extends AbstractBuilder
   implements XMBeanConstants
{

   // Constants -----------------------------------------------------
   
   //
   // Below the XML element names used in the document instances.
   //
   private final static String GET_METHOD = "getMethod";
   private final static String SET_METHOD = "setMethod";
   private final static String PERSIST_POLICY = "persistPolicy";
   private final static String PERSIST_PERIOD = "persistPeriod";
   private final static String PERSIST_NAME = "persistName";
   private final static String PERSIST_LOCATION = "persistLocation";
   private final static String CURRENCY_TIME_LIMIT = "currencyTimeLimit";
   private final static String ON_UPDATE = "OnUpdate";
   private final static String NO_MORE_OFTEN_THAN = "NoMoreOftenThan";
   private final static String NEVER = "Never";
   private final static String ON_TIMER = "OnTimer";

   // Attributes ----------------------------------------------------
   
   /**
    * The URL for the XML document instance.
    */
   private URL url                 = null;
   
   /**
    * Name of the class this Model MBean represents.
    */
   private String resourceClassName = null;
   
   /**
    * Name of the Model MBean implementation class.
    */
   private String mmbClassName     = null;
   
   
   // Constructors --------------------------------------------------

   /**
    * Initializes the builder.
    *
    * @param mmbClassName      name of the Model MBean implementation class
    * @param resourceClassName name of the resource class this Model MBean represents
    * @param url               URL to the XML definition of the management interface
    */
   public XMBeanMetaData(String mmbClassName, String resourceClassName, URL url)
   {
      super();
      
      this.url               = url;
      this.resourceClassName = resourceClassName;
      this.mmbClassName      = mmbClassName;
   }

   /**
    * Initializes the builder.
    *
    * @param mmbClassName      name of the Model MBean implementation class
    * @param resourceClassName name of the resource class this Model MBean represents
    * @param url               URL to the XML definition of the management interface
    *
    * @throws MalformedURLException if the URL string could not be resolved
    */   
   public XMBeanMetaData(String mmbClassName, String resourceClassName, String url) throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url));
   }

   /**
    * Initializes the builder.
    *
    * @param mmbClassName      name of the Model MBean implementation class
    * @param resourceClassName name of the resource class this Model MBean represents
    * @param url               URL to the XML definition of the management interface
    * @param properties        configuration properties for this builder
    */      
   public XMBeanMetaData(String mmbClassName, String resourceClassName, URL url, Map properties)
   {
      this(mmbClassName, resourceClassName, url);
      setProperties(properties);
   }

   /**
    * Initializes the builder.
    *
    * @param mmbClassName      name of the Model MBean implementation class
    * @param resourceClassName name of the resource class this Model MBean represents
    * @param url               URL to the XML definition of the management interface
    * @param properties        configuration properties for this builder
    *
    * @throws MalformedURLException if the URL string could not be resolved
    */         
   public XMBeanMetaData(String mmbClassName, String resourceClassName,
                         String url, Map properties) throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url), properties);
   }

   
   // MetaDataBuilder interface implementation ----------------------

   public MBeanInfo build() throws NotCompliantMBeanException
   {
      try
      {
         // by default, let JAXP pick the SAX parser
         SAXBuilder builder = null;
         
         // check if user wants to override the SAX parser property
         if (properties.get(SAX_PARSER) != null)
            builder = new SAXBuilder(getStringProperty(SAX_PARSER));
         else
            builder = new SAXBuilder();
            
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
            // FIXME: log the exception (warning)
            
            // fall through, use the default value
         }
         
         // get the root and start parsing...   
         Element root = builder.build(url).getRootElement();
         List constructors = root.getChildren("constructor");
         List operations = root.getChildren("operation");
         List attributes = root.getChildren("attribute");
         List notifications = root.getChildren("notifications");
         String description = root.getChildText("description");

         Attribute persistPolicy = root.getAttribute(PERSIST_POLICY);
         Attribute persistPeriod = root.getAttribute(PERSIST_PERIOD);
         Attribute persistLocation = root.getAttribute(PERSIST_LOCATION);
         Attribute persistName = root.getAttribute(PERSIST_NAME);
         Attribute currTimeLimit = root.getAttribute(CURRENCY_TIME_LIMIT);

         Descriptor descr = new DescriptorSupport();
         descr.setField("name", mmbClassName);
         descr.setField("descriptorType", "mbean");

         if (persistPolicy != null)
            descr.setField(PERSIST_POLICY, persistPolicy.getValue());
         if (persistPeriod != null)
            descr.setField(PERSIST_PERIOD, persistPeriod.getValue());
         if (persistLocation != null)
            descr.setField(PERSIST_LOCATION, persistLocation.getValue());
         if (persistName != null)
            descr.setField(PERSIST_NAME, persistName.getValue());
         if (currTimeLimit != null)
            descr.setField(CURRENCY_TIME_LIMIT, currTimeLimit.getValue());

         ModelMBeanInfo info = buildMBeanMetaData(
            description, constructors, operations,
            attributes, notifications, descr
         );

         return (MBeanInfo) info;
      }
      catch (IOException e)
      {
         throw new NotCompliantMBeanException(
            "IO Error parsing the XML file: "
               + url
               + ". Cause: "
               + e.toString());
      }
      catch (JDOMException e)
      {
         throw new NotCompliantMBeanException(
            "Error parsing the XML file: "
               + url
               + ". Cause: "
               + ((e.getCause() == null)
                  ? e.toString()
                  : e.getCause().toString()));
      }
   }


   // Protected -----------------------------------------------------

   /**
    * Parses the elements under the root and turns them into a Model MBean info
    * instance. Subclasses may override if they need to handle additional 
    * elements under the root <tt>&lt;mbean&gt;</tt>.
    *
    * @param   description    the MBean description extracted from the document
    *                         instance.
    * @param   constructors   list of <tt>&lt;constructor&gt;</tt> elements extracted
    *                         from the document instance.
    * @param   operations     list of <tt>&lt;operation&gt;</tt> elements extracted
    *                         from the document instance.
    * @param   attributes     list of <tt>&lt;attributes&gt;</tt> elements extracted
    *                         from the document instance.
    * @param   notifications  list of <tt>&lt;notification&gt;</tt> elements
    *                         extracted from the document instance.
    * @param   descr          MBean descriptor
    *
    * @return  initialized Model MBean info
    */
   protected ModelMBeanInfo buildMBeanMetaData(String description, List constructors,
             List operations, List attributes, List notifications, Descriptor descr)
   {

      ModelMBeanOperationInfo[] operInfo     = buildOperationInfo(operations);
      ModelMBeanAttributeInfo[] attrInfo     = buildAttributeInfo(attributes);
      ModelMBeanConstructorInfo[] constrInfo = buildConstructorInfo(constructors);
      ModelMBeanNotificationInfo[] notifInfo = buildNotificationInfo(notifications);

      ModelMBeanInfo info = new ModelMBeanInfoSupport(
         mmbClassName, description, attrInfo, constrInfo, operInfo, notifInfo, descr
      );

      return info;
   }

   /**
    * Parses the contents of an <tt>&lt;constructor&gt;</tt> element. Subclasses
    * may override if they need to handle additional nested elements or arguments.
    *
    * @param   constructors   a list of <tt>&lt;constructor&gt;</tt> elements
    *                         extracted from the document instance
    *
    * @return  initialized constructor info
    */
   protected ModelMBeanConstructorInfo[] buildConstructorInfo(List constructors)
   {

      Iterator it = constructors.iterator();
      List infos = new ArrayList();

      while (it.hasNext())
      {
         Element constr = (Element) it.next();
         String name = constr.getChildTextTrim("name");
         String descr = constr.getChildTextTrim("description");
         List params = constr.getChildren("parameter");

         MBeanParameterInfo[] paramInfo = buildParameterInfo(params);
         ModelMBeanConstructorInfo info = new ModelMBeanConstructorInfo(name, descr, paramInfo);

         infos.add(info);
      }

      return (ModelMBeanConstructorInfo[]) infos.toArray(new ModelMBeanConstructorInfo[0]);
   }

   /**
    * Parses the contents of an <tt>&lt;operation&gt;</tt> element. Subclasses
    * may override if they need to handle additional nested elements or arguments.
    *
    * @param   operations  a list of <tt>&lt;operation&gt;</tt> elements
    *                      extracted from the document instance
    *
    * @return  initialized operation info
    */
   protected ModelMBeanOperationInfo[] buildOperationInfo(List operations)
   {

      Iterator it = operations.iterator();
      List infos = new ArrayList();

      while (it.hasNext())
      {
         Element oper = (Element) it.next();
         String name = oper.getChildTextTrim("name");
         String descr = oper.getChildTextTrim("description");
         String type = oper.getChildTextTrim("return-type");
         String impact = oper.getChildTextTrim("impact");
         List params = oper.getChildren("parameter");

         MBeanParameterInfo[] paramInfo = buildParameterInfo(params);

         // defaults to ACTION_INFO
         int operImpact = MBeanOperationInfo.ACTION_INFO;

         if (impact != null)
         {
            if (impact.equals("INFO"))
               operImpact = MBeanOperationInfo.INFO;
            else if (impact.equals("ACTION"))
               operImpact = MBeanOperationInfo.ACTION;
            else if (impact.equals("ACTION_INFO"))
               operImpact = MBeanOperationInfo.ACTION_INFO;
         }

         // default return-type is void
         if (type == null)
            type = "void";

         ModelMBeanOperationInfo info = new ModelMBeanOperationInfo(
            name, descr, paramInfo, type, operImpact
         );

         infos.add(info);
      }

      return (ModelMBeanOperationInfo[]) infos.toArray(new ModelMBeanOperationInfo[0]);
   }


   /**
    * Parses the contents of an <tt>&lt;notification&gt;</tt> element. Subclasses
    * may override if they need to handle additional nested elements or arguments.
    *
    * @param   notifications  a list of <tt>&lt;notification&gt;</tt> elements
    *                         extraced from the document instance
    *
    * @return  initialized notification info
    */
   protected ModelMBeanNotificationInfo[] buildNotificationInfo(List notifications)
   {

      Iterator it = notifications.iterator();
      List infos = new ArrayList();

      while (it.hasNext())
      {
         Element notif = (Element) it.next();
         String name = notif.getChildTextTrim("name");
         String descr = notif.getChildTextTrim("description");
         List notifTypes = notif.getChildren("notification-type");

         Iterator iterator = notifTypes.iterator();
         List types = new ArrayList();

         while (iterator.hasNext())
         {
            Element type = (Element) iterator.next();
            types.add(type.getTextTrim());
         }

         ModelMBeanNotificationInfo info = new ModelMBeanNotificationInfo(
            (String[]) types.toArray(), name, descr
         );

         infos.add(info);
      }

      return (ModelMBeanNotificationInfo[]) infos.toArray(
         new ModelMBeanNotificationInfo[0]
      );
   }

   /**
    * Parses the contents of an <tt>&lt;attribute&gt;</tt> element. Subclasses
    * may override if they need to handle additional nested elements or arguments.
    *
    * @param   attributes  a list of <tt>&lt;attribute&gt;</tt> elements
    *                      extracted from the document instance
    *
    * @return  initialized attribute info
    */
   protected ModelMBeanAttributeInfo[] buildAttributeInfo(List attributes)
   {

      Iterator it = attributes.iterator();
      List infos = new ArrayList();

      while (it.hasNext())
      {
         Element attr = (Element) it.next();
         String name = attr.getChildTextTrim("name");
         String description = attr.getChildTextTrim("description");
         String type = attr.getChildTextTrim("type");
         String access = attr.getChildTextTrim("access");

         Attribute persistPolicy = attr.getAttribute(PERSIST_POLICY);
         Attribute persistPeriod = attr.getAttribute(PERSIST_PERIOD);
         Attribute setMethod = attr.getAttribute(SET_METHOD);
         Attribute getMethod = attr.getAttribute(GET_METHOD);
         Attribute currTimeLimit = attr.getAttribute(CURRENCY_TIME_LIMIT);

         Descriptor descr = new DescriptorSupport();
         descr.setField("name", name);
         descr.setField("descriptorType", "attribute");

         if (persistPolicy != null)
            descr.setField(PERSIST_POLICY, persistPolicy.getValue());
         if (persistPeriod != null)
            descr.setField(PERSIST_PERIOD, persistPeriod.getValue());
         if (setMethod != null)
            descr.setField(SET_METHOD, setMethod.getValue());
         if (getMethod != null)
            descr.setField(GET_METHOD, getMethod.getValue());
         if (currTimeLimit != null)
            descr.setField(CURRENCY_TIME_LIMIT, currTimeLimit.getValue());

         // if no method mapping, enable caching automatically
         if (setMethod == null && getMethod == null && currTimeLimit == null) 
            descr.setField(CURRENCY_TIME_LIMIT, "-1");         
            
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

      return (ModelMBeanAttributeInfo[]) infos.toArray(new ModelMBeanAttributeInfo[0]);
   }

   /**
    * Parses the contents of an <tt>&lt;parameter&gt;</tt> element. Subclasses
    * may override if they need to handle additional nested elements or arguments.
    *
    * @param   operations  a list of <tt>&lt;parameter&gt;</tt> elements
    *                      extracted from the document instance
    *
    * @return  initialized parameter info
    */
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

}



