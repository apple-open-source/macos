/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import java.net.URL;
import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;
import java.lang.reflect.Constructor;
import java.lang.reflect.UndeclaredThrowableException;
import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.service.ServiceConstants;
import org.jboss.util.Classes;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.Attr;

/** 
 * A helper class for the controller.
 *   
 * @see Service
 * 
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.6.2.4 $
 */
public class ServiceCreator 
{
   // Attributes ----------------------------------------------------
   private static final String XMBEAN_CODE = "org.jboss.mx.modelmbean.XMBean";
   /** Instance logger. */
   private static final Logger log = Logger.getLogger(ServiceCreator.class);
   
   private MBeanServer server;
   
   public ServiceCreator(final MBeanServer server)
   {
      this.server = server;
   }
   
   /**
    * Parses the given configuration document and creates MBean
    * instances in the current MBean server.
    *
    * @param configuration     The configuration document.
    *
    * @throws ConfigurationException   The configuration document contains
    *                                  invalid or missing syntax.
    * @throws Exception                Failed for some other reason.
    */
   public ObjectInstance install(ObjectName mbeanName, ObjectName loaderName,
      Element mbeanElement) throws Exception
   {
      if (server.isRegistered(mbeanName))
      {
         throw new DeploymentException("Trying to install an already registered mbean: " + mbeanName);
      }
      // If class is given, instantiate it
      String code = mbeanElement.getAttribute("code");
      if ( code == null || "".equals(code))
      {
         throw new ConfigurationException("missing 'code' attribute");
      }

      // get the constructor params/sig to use
      ConstructorInfo constructor = ConstructorInfo.create(mbeanElement);
		
      // Check for xmbean specific attributes
      String xmbeandd = null;
      Attr xmbeanddAttr = mbeanElement.getAttributeNode("xmbean-dd");
      if( xmbeanddAttr != null )
         xmbeandd = xmbeanddAttr.getValue();
      String xmbeanCode = mbeanElement.getAttribute("xmbean-code");
      if( xmbeanCode.length() == 0 )
         xmbeanCode = XMBEAN_CODE;

      // Create the mbean instance
      ObjectInstance instance = null;
      try
      {
         if ( xmbeandd == null )
         {
            // This is a standard or dynamic mbean
            log.debug("About to create bean: " + mbeanName + " with code: " + code);
            instance = server.createMBean(code,
                                          mbeanName,
                                          loaderName,
                                          constructor.params,
                                          constructor.signature);
         } // end of if ()
         else if( xmbeandd.length() == 0 )
         {
            // This is an xmbean with an embedded mbean descriptor
            log.debug("About to create xmbean object: " + mbeanName
               + " with code: " + code + " with embedded descriptor");
            //xmbean: construct object first.
            Object resource = server.instantiate(code, loaderName,
                  constructor.params, constructor.signature);

            NodeList mbeans = mbeanElement.getElementsByTagName("xmbean");
            if( mbeans.getLength() == 0 )
               throw new ConfigurationException("No nested mbean element given for xmbean");
            Element mbeanDescriptor = (Element) mbeans.item(0);
            Object[] args = {resource, mbeanDescriptor,
                             ServiceConstants.PUBLIC_JBOSSMX_XMBEAN_DTD_1_0};
            String[] sig = {Object.class.getName(), Element.class.getName(),
                            String.class.getName()};
            instance = server.createMBean(xmbeanCode,
                                          mbeanName,
                                          loaderName,
                                          args,
                                          sig);
         }
         else
         {
            // This is an xmbean with an external descriptor
            log.debug("About to create xmbean object: " + mbeanName
               + " with code: " + code + " with descriptor: "+xmbeandd);
            //xmbean: construct object first.
            Object resource = server.instantiate(code, loaderName,
                  constructor.params, constructor.signature);
            // Try to find the dd first as a resource then as a URL
            URL xmbeanddUrl = null;
            try
            {
               xmbeanddUrl = resource.getClass().getClassLoader().getResource(xmbeandd);
            }
            catch (Exception e)
            {
            } // end of try-catch
            if (xmbeanddUrl == null)
            {
               xmbeanddUrl = new URL(xmbeandd);
            } // end of if ()

            //now create the mbean
            Object[] args = {resource, xmbeanddUrl};
            String[] sig = {Object.class.getName(), URL.class.getName()};
            instance = server.createMBean(xmbeanCode,
                                          mbeanName,
                                          loaderName,
                                          args,
                                          sig);
         } // end of else
      }
      catch (Throwable e)
      {
         Throwable newE = JMXExceptionDecoder.decode(e);
         if (newE instanceof ClassNotFoundException)
         {
            log.debug("Class not found for mbean: " + mbeanName);
            throw (ClassNotFoundException)newE;
         }

         // didn't work, unregister in case the jmx agent is screwed.
         try 
         {
            server.unregisterMBean(mbeanName);
         }
         catch (Throwable ignore)
         {
         }

         if (newE instanceof Exception)
         {
            throw (Exception)newE;
         } // end of if ()
         throw new UndeclaredThrowableException(newE);
      }

      log.debug("Created bean: "+mbeanName);
      return instance;
   }

   public void remove(ObjectName name) throws Exception
   {
      // add defaut domain if there isn't one in this name
      String domain = name.getDomain();
      if (domain == null || "".equals(domain))
      {
         name = new ObjectName(server.getDefaultDomain() + name);
      }

      // Remove the MBean from the MBeanServer
      server.unregisterMBean(name);
   }	
   
   /**
    * Provides a wrapper around the information about which constructor
    * that MBeanServer should use to construct a MBean.
    * Please note that only basic datatypes (type is then the same as
    * you use to declare it "short", "int", "float" etc.) and any class
    * having a constructor taking a single "String" as only parameter.
    *
    * <p>XML syntax for contructor:
    *   <pre>
    *      <constructor>
    *         <arg type="xxx" value="yyy"/>
    *         ...
    *         <arg type="xxx" value="yyy"/>
    *      </constructor>
    *   </pre>
    */
   private static class ConstructorInfo
   {
      /** An empty parameters list. */
      public static final Object EMPTY_PARAMS[] = {};
      
      /** An signature list. */
      public static final String EMPTY_SIGNATURE[] = {};
      
      /** The constructor signature. */
      public String[] signature = EMPTY_SIGNATURE;
      
      /** The constructor parameters. */
      public Object[] params = EMPTY_PARAMS;
      
      /**
       * Create a ConstructorInfo object for the given configuration.
       *
       * @param element   The element to build info for.
       * @return          A constructor information object.
       *
       * @throws ConfigurationException   Failed to create info object.
       */
      public static ConstructorInfo create(Element element)
         throws ConfigurationException
      {
         ConstructorInfo info = new ConstructorInfo();
         NodeList list = element.getElementsByTagName("constructor");
         if (list.getLength() > 1 && list.item(0).getParentNode() == element)
         {
            throw new ConfigurationException
            ("only one <constructor> element may be defined");
         }
         else if (list.getLength() == 1)
         {
            element = (Element)list.item(0);
            
            // get all of the "arg" elements
            list = element.getElementsByTagName("arg");
            int length = list.getLength();
            info.params = new Object[length];
            info.signature = new String[length];
            ClassLoader loader = Thread.currentThread().getContextClassLoader();

            // decode the values into params & signature
            for (int j=0; j<length; j++)
            {
               Element arg = (Element)list.item(j);
               String signature = arg.getAttribute("type");
               String value = arg.getAttribute("value");
               Object realValue = value;

               if( signature != null )
               {
                  // See if it is a primitive type first
                  Class typeClass = Classes.getPrimitiveTypeForName(signature);
                  if (typeClass == null)
                  {
                     // Try to load the class
                     try
                     {
                        typeClass = loader.loadClass(signature);
                     }
                     catch (ClassNotFoundException e)
                     {
                        throw new ConfigurationException
                           ("Class not found for type: " + signature, e);
                     }
                  }

                  // Convert the string to the real value
                  PropertyEditor editor = PropertyEditorManager.findEditor(typeClass);
                  if (editor == null)
                  {
                     try
                     {
                        // See if there is a ctor(String) for the type
                        Class[] sig = {String.class};
                        Constructor ctor = typeClass.getConstructor(sig);
                        Object[] args = {value};
                        realValue = ctor.newInstance(args);
                     }
                     catch (Exception e)
                     {
                        throw new ConfigurationException("No property editor for type: " + typeClass);
                     }
                  }
                  else
                  {
                     editor.setAsText(value);
                     realValue = editor.getValue();
                  }
               }
               info.signature[j] = signature;
               info.params[j] = realValue;
            }
         }

         return info;
      }
   }
   
}
