/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.modelmbean;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Iterator;

import javax.management.Descriptor;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanRegistration;
import javax.management.NotCompliantMBeanException;
import javax.management.modelmbean.InvalidTargetObjectTypeException;
import javax.management.modelmbean.ModelMBeanInfo;

import org.jboss.mx.metadata.XMLMetaData;
import org.jboss.mx.metadata.StandardMetaData;
import org.jboss.mx.metadata.MBeanInfoConversion;
import org.jboss.mx.metadata.MetaDataBuilder;

/**
 * XMBean implementation.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author Matt Munz
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.8.4.3 $
 */
public class XMBean
   extends ModelMBeanInvoker
   implements MBeanRegistration, XMBeanConstants
{
   // Constructors --------------------------------------------------

   /**
    * Default constructor for the XMBean Model MBean implementation. This
    * creates an uninitialized Model MBean template.
    */
   public XMBean()
   {
      // spec required constructor
      super();
   }

   /**
    * Creates an XMBean Model MBean implementation with a predefined JMX
    * metadata.
    *
    * @param   info  Model MBean metadata describing this MBean template
    */
   public XMBean(ModelMBeanInfo info) throws MBeanException
   {
      // spec required constructor
      super(info);
   }

   /**
    * Creates a XMBean instance with a given resource object and resource type. <p>
    *
    * This Model MBean implementation supports the following resource types:    <br><pre>
    *
    *   - {@link ModelMBeanConstants#OBJECT_REF OBJECT_REF}
    *   - {@link XMBeanConstants#STANDARD_INTERFACE STANDARD_INTERFACE}
    *   - {@link XMBeanConstants#DESCRIPTOR DESCRIPTOR}
    *   - Any valid URL string to a *.xml file.
    *
    * </pre>
    *
    * <tt><b>OBJECT_REF:</b></tt> resource object can be any Java object. The
    * management interface must be set separately via
    * {@link javax.management.modelmbean.ModelMBean#setModelMBeanInfo setModelMBeanInfo}
    * method.  <p>
    *
    * <tt><b>STANDARD_INTERFACE:</b></tt> the resource object is assumed to
    * follow the Standard MBean naming conventions to expose its management
    * interface, including implementing a <tt>xxxMBean</tt> interface. A
    * corresponding Model MBean metadata is generated for the Model MBean
    * representing this resource type.  <p>
    *
    * <tt><b>DESCRIPTOR:</b></tt> the resource object is wrapped as a part of
    * the {@link javax.management.Descriptor Descriptor} object passed to this
    * Model MBean instance. The descriptor object must contain the mandatory
    * fields {@link XMBeanConstants#RESOURCE_REFERENCE RESOURCE_REFERENCE} and
    * {@link XMBeanConstants#RESOURCE_TYPE RESOURCE_TYPE} that identify the
    * correct resource reference and type used for this Model MBean instance.
    * The descriptor object may also contain additional fields, such as
    * {@link XMBeanConstants#SAX_PARSER SAX_PARSER} and
    * {@link XMBeanConstants#XML_VALIDATION XML_VALIDATION} that are passed as
    * configuration properties for the metadata builder instances. Any
    * additional descriptor fields that match the
    * {@link XMBeanConstants#METADATA_DESCRIPTOR_PREFIX METADATA_DESCRIPTOR_PREFIX}
    * naming pattern will be passed to the builder implementation via its
    * {@link org.jboss.mx.metadata.MetaDataBuilder#setProperty setProperty}
    * method.    <p>
    *
    * <tt><b>URL String:</b></tt> if a resource type string contains an URL
    * that ends with a *.xml file name the resource object is exposed via the
    * XML management interface definition read from this URL. The XML parser
    * implementation is picked based on the schema definition in the XML
    * document.
    *
    * @param   resource     resource object or descriptor
    * @param   resourceType resource type string or URL to *.xml file
    */
   public XMBean(Object resource, String resourceType) throws MBeanException, NotCompliantMBeanException
   {
      try
      {
         HashMap properties = new HashMap();

         if (resourceType.equals(DESCRIPTOR))
         {
            Descriptor d = (Descriptor) resource;

            // get the actual resource type from the descriptor
            resourceType = (String) d.getFieldValue(RESOURCE_TYPE);

            // and the resource reference
            resource = d.getFieldValue(RESOURCE_REFERENCE);

            // extract builder configuration fields
            String[] fields = d.getFieldNames();

            for (int i = 0; i < fields.length; ++i)
            {
               // extract all the fields starting with the METADATA_DESCRIPTOR_PREFIX
               // prefix to a property map that is passed to the builder implementations
               if (fields[i].startsWith(METADATA_DESCRIPTOR_PREFIX))
                  properties.put(fields[i], d.getFieldValue(fields[i]));
            }
         }

         setManagedResource(resource, resourceType);


         // the resource implements a Standard MBean interface
         if (resourceType.equals(STANDARD_INTERFACE))
         {
            // automatically create management operations that the attributes
            // can map to.
            final boolean CREATE_ATTRIBUTE_OPERATION_MAPPING = true;

            // create and configure the builder
            MetaDataBuilder builder = new StandardMetaData(resource);

            // pass the config keys to the builder instance
            for (Iterator it = properties.keySet().iterator(); it.hasNext();)
            {
               String key = (String) it.next();
               builder.setProperty(key, properties.get(key));
            }

            // build the metadata
            MBeanInfo standardInfo = builder.build();

            // StandardMetaData is used by the MBean server to introspect
            // standard MBeans. We need to now turn that Standard metadata into
            // ModelMBean metadata (including operation mapping for attributes)
            ModelMBeanInfo minfo = MBeanInfoConversion.toModelMBeanInfo(standardInfo, CREATE_ATTRIBUTE_OPERATION_MAPPING);
            this.setModelMBeanInfo(minfo);
         }

         // If the resource type string ends with an '.xml' extension attempt
         // to create the metadata with the aggregated XML builder.
         else if (resourceType.endsWith(".xml"))
         {
            // Create and configure the builder. XMLMetaData builder is an
            // aggregate builder that picks the correct schema specific builder
            // based on schema declaration at the beginning of the XML file.

            MetaDataBuilder builder = new XMLMetaData(
               this.getClass().getName(), // MMBean implementation name
               resource.getClass().getName(), // resource class name
               resourceType
            );

            // pass the config keys to the builder instance
            for (Iterator it = properties.keySet().iterator(); it.hasNext();)
            {
               String key = (String) it.next();
               builder.setProperty(key, properties.get(key));
            }

            ModelMBeanInfo minfo = (ModelMBeanInfo) builder.build();
            setModelMBeanInfo(minfo);
         }
         // we must try to load this MBean (as the superclass does), even if only NullPersistence
         // is used - MMM
         load();
      }
      catch (InstanceNotFoundException e)
      {
         throw new MBeanException(e);
      }
      catch (InvalidTargetObjectTypeException e)
      {
         if (resourceType.endsWith(".xml"))
            throw new MBeanException(e, "Malformed URL: " + resourceType);

         throw new MBeanException(e, "Unsupported resource type: " + resourceType);
      }
      catch (MalformedURLException e)
      {
         throw new MBeanException(e, "Malformed URL: " + resourceType);
      }
   }


   public XMBean(Object resource, URL interfaceURL) throws MBeanException, NotCompliantMBeanException
   {
      this(resource, interfaceURL.toString());
   }


   public XMBean(Descriptor descriptor) throws MBeanException, NotCompliantMBeanException
   {
      this(descriptor, DESCRIPTOR);
   }

   public XMBean(Object resource, org.w3c.dom.Element element, String version) throws MBeanException, NotCompliantMBeanException
   {
      //      this(resource, OBJECT_REF);
      try
      {
         setManagedResource(resource, OBJECT_REF);
         MetaDataBuilder builder = new XMLMetaData(
            this.getClass().getName(), // MMBean implementation name
            resource.getClass().getName(), // resource class name
            element,
            version
         );

         ModelMBeanInfo minfo = (ModelMBeanInfo) builder.build();
         setModelMBeanInfo(minfo);
      }
      catch (InstanceNotFoundException e)
      {
         throw new MBeanException(e);
      }
      catch (InvalidTargetObjectTypeException e)
      {
         throw new MBeanException(e, "Unsupported resource type: " + resourceType);
      }
   }

   // Public --------------------------------------------------------

   public boolean isSupportedResourceType(Object resource, String resourceType)
   {
      if (resourceType == null)
         return false;

      if (resourceType.equals(OBJECT_REF))
         return true;
      if (resourceType.equals(STANDARD_INTERFACE))
         return true;
      if (resourceType.equals(DESCRIPTOR))
      {
         if (!(resource instanceof Descriptor))
            return false;

         Descriptor d = (Descriptor) resource;

         if (d.getFieldValue(RESOURCE_REFERENCE) == null)
            return false;

         if (d.getFieldValue(RESOURCE_TYPE) == null)
            return false;

         return true;
      }
      if (resourceType.endsWith(".xml"))
      {
         try
         {
            new URL(resourceType);
            return true;
         }
         catch (MalformedURLException e)
         {
            return false;
         }
      }

      return false;
   }

   // DynamicMBean implementation -----------------------------------
   public MBeanInfo getMBeanInfo()
   {
      return info;
   }

}
