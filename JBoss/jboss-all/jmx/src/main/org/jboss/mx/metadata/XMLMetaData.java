/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.metadata;

import org.jdom.Attribute;
import org.jdom.Document;
import org.jdom.DocType;
import org.jdom.Element;
import org.jdom.JDOMException;
import org.jdom.input.SAXBuilder;

import java.io.IOException;
import java.io.InputStream;

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

import java.net.MalformedURLException;
import java.net.URL;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

import org.jboss.mx.modelmbean.XMBeanConstants;

import org.jboss.mx.service.ServiceConstants;

/**
 * Aggregate builder for XML schemas. This builder implementation is used
 * as an aggregate for all XML based builder implementations. The correct
 * XML parser is picked based on the schema declaration of the XML file.  
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author Matt Munz
 */
public class XMLMetaData
   extends AbstractBuilder
   implements ServiceConstants, XMBeanConstants
{

   // Attributes ----------------------------------------------------
   private static final int NO_VERSION = -1;
   private static final int XMBEAN = 0;
   private static final int JBOSS_XMBEAN_1_0 = 1;
   private static final int JBOSS_XMBEAN_1_2 = 2;
   private static final int JBOSS_XMBEAN_1_1 = 3;
   


   /**
    * The URL for the XML file.
    */
   private URL url                  = null;

   private org.w3c.dom.Element element;

   private String versionString;
   
   /**
    * The class name of the resource class this Model MBean represents.
    */
   private String resourceClassName = null;

   /**
    * The class name of the Model MBean implementation class.
    */
   private String mmbClassName      = null;
   
   
   // Constructors --------------------------------------------------

   /**
    * Constructs an aggregate XML builder implementation.
    *
    * @param   mmbClassName         the class name of the Model MBean
    *                               implementation
    * @param   resourceClassName    the class name of the resource object the
    *                               Model MBean represents
    * @param   url                  the URL for the XML definition of the
    *                               management interface
    */
   public XMLMetaData(String mmbClassName, String resourceClassName, URL url)
   {
      super();
      
      this.url               = url;
      this.mmbClassName      = mmbClassName;
      this.resourceClassName = resourceClassName;
   }
   
   /**
    * Constructs an aggregate XML builder implementation.
    *
    * @param   mmbClassName         the class name of the Model MBean
    *                               implementation
    * @param   resourceClassName    the class name of the resource object the
    *                               Model MBean represents
    * @param   url                  the URL for the XML definition of the
    *                               management interface
    *
    * @throws  MalformedURLException if the URL string could not be resolved
    */
   public XMLMetaData(String mmbClassName, String resourceClassName, String url)
      throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url));
   }

   /**
    * Constructs an aggregate XML builder implementation.
    *
    * @param   mmbClassName         the class name of the Model MBean
    *                               implementation
    * @param   resourceClassName    the class name of the resource object the
    *                               Model MBean represents
    * @param   url                  the URL for the XML definition of the
    *                               management interface
    * @param   properties           Map of configuration properties for this
    *                               builder. These properties will be passed
    *                               to the appropriate XML schema specific builder
    *                               when it is created.
    */
   public XMLMetaData(String mmbClassName, String resourceClassName, URL url, Map properties)
   {
      this(mmbClassName, resourceClassName, url);
      setProperties(properties);
   }
   
   /**
    * Constructs an aggregate XML builder implementation.
    *
    * @param   mmbClassName         the class name of the Model MBean
    *                               implementation
    * @param   resourceClassName    the class name of the resource object the
    *                               Model MBean represents
    * @param   url                  the URL for the XML definition of the
    *                               management interface
    * @param   properties           Map of configuration properties for this
    *                               builder. These properties will be passed
    *                               to the appropriate XML schema specific builder
    *                               when it is created.
    *
    * @throws  MalformedURLException if the URL string could not be resolved
    */
   public XMLMetaData(String mmbClassName, String resourceClassName,
                      String url, Map properties) throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url), properties);
   }

   /**
    * Creates a new <code>XMLMetaData</code> instance using an explicit DOM element 
    * as the configuration source, and requiring an explicit version indicator.
    * The version should be the PublicID for the dtd or (worse) the dtd url.
    *
    * @param mmbClassName a <code>String</code> value
    * @param resourceClassName a <code>String</code> value
    * @param element an <code>org.w3c.dom.Element</code> value
    * @param version a <code>String</code> value
    */
   public XMLMetaData(String mmbClassName, String resourceClassName, org.w3c.dom.Element element, String version)
   {
      super();
      this.mmbClassName = mmbClassName;
      this.resourceClassName = resourceClassName;
      this.element = element;
      versionString = version;
   }

   
   // MetaDataBuilder implementation --------------------------------
   /**
    * Constructs the Model MBean metadata. This implementation reads the 
    * document type definition from the beginning of the XML file and picks
    * a corresponding XML builder based on the schema name. In case no
    * document type is defined the latest schema builder for this JBossMX
    * release is used. <p>
    *
    * The SAX parser implementation is selected by default based on JAXP
    * configuration. If you want to use JAXP to select the parser, you can
    * set the system property <tt>"javax.xml.parsers.SAXParserFactory"</tt>.
    * For example, to use Xerces you might define:   <br><pre>
    *
    *    java -Djavax.xml.parsers.SAXParserFactory=org.apache.xerces.jaxp.SAXParserFactoryImpl ...
    *
    * </pre>
    *
    * In case you can't or don't want to use JAXP to configure the SAX parser
    * implementation you can override the SAX parser implementation by setting
    * an MBean descriptor field {@link XMBeanConstants#SAX_PARSER} to the
    * parser class string value.
    *
    * @return initialized MBean info
    * @throws NotCompliantMBeanException if there were errors building the 
    *         MBean info from the given XML file.
    */
   public MBeanInfo build() throws NotCompliantMBeanException
   {
      try
      {
         int version = NO_VERSION;         
         
         if (versionString == null)
	     {
            // by default, let JAXP pick the SAX parser
            SAXBuilder builder = new SAXBuilder();
         
            // check if user wants to override the SAX parser property
            if (properties.get(SAX_PARSER) == null)
            {
               // by default, let JAXP pick the SAX parser. Validation off.
               builder = new SAXBuilder(false);
            }
            else
            {
               //User's SAX parser, validation off.
               builder = new SAXBuilder(getStringProperty(SAX_PARSER), false);
            } // end of else
         
            //supply local resolver for our dtds
            builder.setEntityResolver(new XMBeanEntityResolver());
         
            // try to extract either SYSTEM or PUBLIC ID and guess the schema
            // specific builder based on the schema file name
            InputStream docStream = url.openStream();
            Document doc = builder.build(docStream);
            docStream.close();
            DocType type = doc.getDocType();

            version = validateVersionString(type.getPublicID());
            if (version == NO_VERSION) 
            {
               version = validateVersionString(type.getSystemID());               
            } // end of if ()
            
            /*
            versionString = type.getSystemID();

            // if we can't find SYSTEM ID try to get PUBLIC ID
	        if (versionString == null)
	        versionString = type.getPublicID();
            */
	     }
	     else
         {
            version = validateVersionString(versionString);
         } // end of else
         // These are the known schemas for us. Pick the correct one based on
         // schema or default to the latest.docURL.endsWith(JBOSSMX_XMBEAN_DTD_1_0)
         if (version == JBOSS_XMBEAN_1_0 || version == JBOSS_XMBEAN_1_1)
         {
            // jboss_xmbean_1_0.dtd
            
            // this schema has been deprecated, still keeping it here for
            // backwards compatibility
	        //well, months later it's still the only one implemented...
	        if (element == null)
	        {
	           return new JBossXMBean10(mmbClassName, resourceClassName, url, properties).build();
	        }
	        else 
	        {
	           //Only one implemented so far, so I won't write code that 
	           //won't work for the others.
	           return new JBossXMBean10(mmbClassName, resourceClassName, element).build();
	        }
         }
         else if (version == XMBEAN)
         {
            // xmbean.dtd
            
            return new XMBeanMetaData(mmbClassName, resourceClassName, url, properties).build();
         }
         else if (version == JBOSS_XMBEAN_1_2) 
         {
            // defaults to the latest JBossMX XMBean schema
            return new JBossXMBean12(mmbClassName, resourceClassName, url, properties).build();
         }
         else
         {
            throw new NotCompliantMBeanException("Unknown xmbean type " + versionString);            
         } // end of else
         
      }
      catch (JDOMException e)
      {
         System.out.println("JDOM Exception: " + e);
         e.printStackTrace();
         throw new NotCompliantMBeanException("Error parsing the XML file, from XMLMetaData: " + ((e.getCause() == null) ? e.toString() : e.getCause().toString()));
      }
      catch (IOException e)
      {
         // FIXME: log the stack trace?
         //e.printStackTrace();
         //jdk1.4 throw new NotCompliantMBeanException("Error parsing the XML file: " + ((e.getCause() == null) ? e.toString() : e.getCause().toString()));
         throw new NotCompliantMBeanException("Error parsing the XML file: " +  e.toString());
      }
   }
   
   private int validateVersionString(String versionString)
   {  	
      if (PUBLIC_JBOSSMX_XMBEAN_DTD_1_0.equals(versionString)) 
      {
         return JBOSS_XMBEAN_1_0;
      } // end of if ()
      if (versionString != null && versionString.endsWith(JBOSSMX_XMBEAN_DTD_1_0))
      {
         return JBOSS_XMBEAN_1_0;
      } // end of if ()
      if (PUBLIC_JBOSSMX_XMBEAN_DTD_1_1.equals(versionString)) 
      {
         return JBOSS_XMBEAN_1_1;
      } // end of if ()
      if (versionString != null && versionString.endsWith(JBOSSMX_XMBEAN_DTD_1_1))
      {
         return JBOSS_XMBEAN_1_1;
      } // end of if ()
      
      if (versionString != null && versionString.endsWith(XMBEAN_DTD))
      {
         return XMBEAN;
      } // end of if ()
      //There is nothing defined for jboss xmbean 1.2, so we can't recognize it.
      return NO_VERSION;
   }
      

}


