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
import java.util.Map;

import javax.management.MBeanInfo;
import javax.management.NotCompliantMBeanException;

import org.jboss.logging.Logger;
import org.jboss.mx.modelmbean.XMBeanConstants;
import org.jdom.Element;
import org.jdom.JDOMException;
import org.jdom.input.SAXBuilder;

/**
 * @TODO
 */
public class JBossXMBean12
   extends AbstractBuilder
   implements XMBeanConstants
{
   private static Logger log = Logger.getLogger(JBossXMBean12.class);

   // Attributes ----------------------------------------------------
   
   /**
    * URL for the XML definition of the management interface
    */
   private URL url                  = null;
   
   /**
    * The class name of the Model MBean implementation class.
    */
   private String mmbClassName      = null;
   
   /**
    * The class name of the resource object represented by this Model MBean.
    */
   private String resourceClassName = null;
    

  
   // Constructors --------------------------------------------------

   /**
    * Initialized a parser for the JBossMX 1.2 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    */
   public JBossXMBean12(String mmbClassName, String resourceClassName, URL url)
   {
      super();
      
      this.url               = url;
      this.mmbClassName      = mmbClassName;
      this.resourceClassName = resourceClassName;
   }

   /**
    * Initialized a parser for the JBossMX 1.2 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    *
    * @throws MalformedURLException if the given management interface URL cannot
    *         be resolved
    */
   public JBossXMBean12(String mmbClassName, String resourceClassName, String url) throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url));
   }

   /**
    * Initialized a parser for the JBossMX 1.2 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    */
   public JBossXMBean12(String mmbClassName, String resourceClassName, URL url, Map properties)
   {
      this(mmbClassName, resourceClassName, url);
      setProperties(properties);
   }

   /**
    * Initialized a parser for the JBossMX 1.2 XMBean schema.
    *
    * @param   mmbClassName      the name of the Model MBean implementation class
    * @param   resourceClassName the name of the resource class this Model
    *                            MBean represents
    * @param   url               URL to the XMBean management interface definition
    *
    * @throws MalformedURLException if the given management interface URL cannot
    *         be resolved
    */
   public JBossXMBean12(String mmbClassName, String resourceClassName,
                        String url, Map properties) throws MalformedURLException
   {
      this(mmbClassName, resourceClassName, new URL(url), properties);
   }


   // MetaDataBuilder implementation --------------------------------

   public MBeanInfo build() throws NotCompliantMBeanException
   {
      try
      {
         // by default, let JAXP pick the SAX parser
         SAXBuilder builder = new SAXBuilder();
         
         // check if user wants to override the SAX parser property
         if (properties.get(SAX_PARSER) != null)
            builder = new SAXBuilder(getStringProperty(SAX_PARSER));
            ///*"org.apache.crimson.parser.XMLReaderImpl"*/);

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
         
         return null;
      }
      catch (IOException e)
      {
         String msg =
            "IO Error parsing the XML file: "
               + url
               + ". Cause: "
               + e.toString();
         log.error(msg, e);
         throw new NotCompliantMBeanException(msg);
      }
      catch (JDOMException e)
      {
         String msg =
            "Error parsing the XML file: "
               + url
               + ". Cause: "
               + ((e.getCause() == null)
                  ? e.toString()
                  : e.getCause().toString());
         log.error(msg, e);
         throw new NotCompliantMBeanException(msg);
      }
   }

   

}


