/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;
import java.io.StringWriter;
import java.io.Writer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;

import javax.management.Attribute;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.util.Classes;
import org.jboss.util.Strings;
import org.jboss.util.xml.DOMWriter;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;

/**
 * Service configuration helper.
 *
 * @version <tt>$Revision: 1.16.2.9 $</tt>
 * @author  <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author  <a href="mailto:hiram@jboss.org">Hiram Chirino</a>
 * @author  <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 *
 */
public class ServiceConfigurator
{
   /** Augment the PropertyEditorManager search path to incorporate the JBoss
       specific editors. This simply references the PropertyEditors.class to
       invoke its static initialization block.
   */
   static
   {
      Class c = org.jboss.util.propertyeditor.PropertyEditors.class;
   }

   /** The MBean server which this service is registered in. */
   private final MBeanServer server;
   private final ServiceController serviceController;
   private final ServiceCreator serviceCreator;

   /** The instance logger. */
   private final Logger log = Logger.getLogger(getClass());

   public ServiceConfigurator(final MBeanServer server,
                              final ServiceController serviceController,
                              final ServiceCreator serviceCreator)
   {
      this.server = server;
      this.serviceController = serviceController;
      this.serviceCreator = serviceCreator;
   }

   /**
    * The <code>install</code> method iterates through the mbean tags in the
    * supplied xml configuration and creates and configures the mbeans shown.
    * The mbean configuration can be nested.
    *
    * @param config the xml <code>Element</code> containing the configuration of the
    *               mbeans to create and configure.
    * @return a <code>List</code> of ObjectNames of created mbeans.
    * @exception DeploymentException if an error occurs
    */
   public List install(Element config, ObjectName loaderName) throws DeploymentException
   {
      List mbeans = new ArrayList();
      try
      {
         if (config.getTagName().equals("mbean"))
         {
            internalInstall(config, mbeans, loaderName);
         }
         else
         {
            NodeList nl = config.getChildNodes();

            for (int i = 0; i < nl.getLength(); i++)
            {
               if (nl.item(i).getNodeType() == Node.ELEMENT_NODE)
               {
                  Element element = (Element)nl.item(i);
                  if (element.getTagName().equals("mbean"))
                  {
                     Element mbean = (Element)nl.item(i);
                     internalInstall(mbean, mbeans, loaderName);
                  } // end of if ()
               } // end of if ()
            }//end of for
         } //end of else
         return mbeans;
      }
      catch (Exception e)
      {
         for (ListIterator li = mbeans.listIterator(mbeans.size()); li.hasPrevious();)
         {
            ObjectName mbean = (ObjectName)li.previous();
            try
            {
               serviceCreator.remove(mbean);
            }
            catch (Exception n)
            {
               log.error("exception removing mbean after failed deployment: " + mbean, n);
            }
         }

         if (e instanceof DeploymentException)
            throw (DeploymentException)e;

         throw new DeploymentException(e);
      }
   }

   private ObjectName internalInstall(Element mbeanElement, List mbeans,
                                      ObjectName loaderName) throws Exception
   {
      ObjectInstance instance = null;
      ObjectName mbeanName = parseObjectName(mbeanElement);

      try
      {
         instance = serviceCreator.install(mbeanName, loaderName, mbeanElement);
      }
      catch (ClassNotFoundException cnfe)
      {
         log.warn("Failed to complete install", cnfe);
         serviceController.registerWaitingForClass(mbeanName, mbeanElement);
         mbeans.add(mbeanName);//so undeploy of package will remove "waiting for class" mbeans.
         return mbeanName;
      }

      serviceController.registerMBeanClassName(instance);

      // just in case it changed...
      mbeanName = instance.getObjectName();

      mbeans.add(mbeanName);
      if (mbeanName != null)
      {
         ServiceContext ctx = serviceController.createServiceContext(mbeanName);
         try
         {
            configure(mbeanName, loaderName, mbeanElement, mbeans);
            ctx.state = ServiceContext.CONFIGURED;
            ctx.problem = null;
         }
         catch (Exception e)
         {
            ctx.state = ServiceContext.FAILED;
            ctx.problem = e;
            log.info("Problem configuring service " + mbeanName, e);
            //throw e;
         }
      }

      return mbeanName;
   }

   /**
    * The <code>configure</code> method configures an mbean based on the xml element configuration
    * passed in.  Three formats are supported:
    * &lt;attribute name="(name)"&gt;(value)&lt;/attribute&gt;
    * &lt;depends optional-attribute-name="(name)"&gt;(object name of mbean referenced)&lt;/depends&gt;
    * &lt;depends-list optional-attribute-name="(name)"&gt;
    * [list of]  &lt;/depends-list-element&gt;(object name)&lt;/depends-list-element&gt;
    * &lt;/depends-list&gt;
    *
    * The last two can include nested mbean configurations or ObjectNames.
    * SIDE-EFFECT: adds all mbeans this one depends on to the ServiceContext structures.
    *
    * @param mbeanElement an <code>Element</code> value
    *
    * @throws Exception if an error occurs
    */
   protected void configure(ObjectName objectName, ObjectName loaderName,
                            Element mbeanElement, List mbeans)
      throws Exception
   {
      // Set configuration to MBeans from XML

      boolean debug = log.isDebugEnabled();

      MBeanInfo info;
      try
      {
         info = server.getMBeanInfo(objectName);
      }
      catch (InstanceNotFoundException e)
      {
         // The MBean is no longer available
         throw new DeploymentException("trying to configure nonexistent mbean: " + objectName);
      }
      catch (Exception e)
      {
         throw new DeploymentException("Could not get mbeanInfo", JMXExceptionDecoder.decode(e));
      } // end of catch

      if (info == null)
      {
         throw new DeploymentException("MBeanInfo is null for mbean: " + objectName);
      } // end of if ()

      // Get the classloader for loading attribute classes.
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      // Initialize the mbean using the configuration supplied defaults
      MBeanAttributeInfo[] attributes = info.getAttributes();
      NodeList attrs = mbeanElement.getChildNodes();
      for (int j = 0; j < attrs.getLength(); j++)
      {
         // skip over non-element nodes
         if (attrs.item(j).getNodeType() != Node.ELEMENT_NODE)
         {
            continue;
         }

         Element element = (Element)attrs.item(j);

         // Set attributes
         if (element.getTagName().equals("attribute"))
         {
            String attributeName = element.getAttribute("name");
            boolean replace = true;
            boolean trim = true;
            String replaceAttr = element.getAttribute("replace");
            if( replaceAttr.length() > 0 )
               replace = Boolean.valueOf(replaceAttr).booleanValue();
            String trimAttr = element.getAttribute("trim");
            if( trimAttr.length() > 0 )
               trim = Boolean.valueOf(trimAttr).booleanValue();

            attrfound:

            if (element.hasChildNodes())
            {
               // Get the attribute value
               String attributeText = getElementContent(element, trim);

               for (int k = 0; k < attributes.length; k++)
               {
                  // skip over non-matching attribute names
                  if (!attributeName.equals(attributes[k].getName()))
                  {
                     continue;
                  }

                  String typeName = attributes[k].getType();

                  // see if it is a primitive type first
                  Class typeClass = Classes.getPrimitiveTypeForName(typeName);
                  if (typeClass == null)
                  {
                     // nope try look up
                     try
                     {
                        typeClass = cl.loadClass(typeName);
                     }
                     catch (ClassNotFoundException e)
                     {
                        throw new DeploymentException
                           ("Class not found for attribute: " + attributeName, e);
                     }
                  }

                  Object value = null;

                  // HRC: Is the attribute type a org.w3c.dom.Element??
                  if (typeClass.equals(Element.class))
                  {
                     // Then we can pass the first child Element of this element
                     NodeList nl = element.getChildNodes();
                     for (int i=0; i < nl.getLength(); i++)
                     {
                        Node n = nl.item(i);
                        if (n.getNodeType() == Node.ELEMENT_NODE)
                        {
                           value = n;
                           break;
                        }
                     }
                  }

                  if (value == null)
                  {
                     PropertyEditor editor = PropertyEditorManager.findEditor(typeClass);
                     if (editor == null)
                     {
                        throw new DeploymentException
                           ("No property editor for attribute: " + attributeName +
                            "; type=" + typeClass);
                     }

                     if( replace == true )
                        attributeText = Strings.replaceProperties(attributeText);
                     editor.setAsText(attributeText);
                     value = editor.getValue();
                  }

                  log.debug(attributeName + " set to " + value + " in " + objectName);
                  setAttribute(objectName, new Attribute(attributeName, value));

                  break attrfound;

               }//for attr names

               throw new DeploymentException("No Attribute found with name: " +  attributeName);
            }//if has children
         }
         //end of "attribute
         else if (element.getTagName().equals("depends"))
         {
            if (!element.hasChildNodes())
            {
               throw new DeploymentException("No ObjectName supplied for depends in  " + objectName);
            }

            String mbeanRefName = element.getAttribute("optional-attribute-name");
            if ("".equals(mbeanRefName))
            {
               mbeanRefName = null;
            } // end of if ()

            String proxyType = element.getAttribute("proxy-type");
            if ("".equals(proxyType))
               proxyType = null;

            // Get the mbeanRef value
            ObjectName dependsObjectName = processDependency(objectName, loaderName, element, mbeans);
            if (debug)
               log.debug("considering " + ((mbeanRefName == null)? "<anonymous>": mbeanRefName.toString()) + " with object name " + dependsObjectName);

            if (mbeanRefName != null)
            {
               Object attribute = dependsObjectName;
               if (proxyType != null)
               {
                  if (mbeanRefName == null)
                     throw new DeploymentException("You cannot use a proxy-type without an optional-attribute-name");
                  if (proxyType.equals("attribute"))
                  {
                     for (int k = 0; k < attributes.length; k++)
                     {
                        // skip over non-matching attribute names
                        if (mbeanRefName.equals(attributes[k].getName()) == false)
                           continue;
                        proxyType = attributes[k].getType();
                        break;
                     }
                     // Didn't find the attribute?
                     if (proxyType.equals("attribute"))
                        throw new DeploymentException("Attribute not found :" + mbeanRefName);             
                  }
                  Class proxyClass = cl.loadClass(proxyType);
                  attribute = MBeanProxyExt.create(proxyClass, dependsObjectName, server);                  
               }

               //if if doesn't exist or has wrong type, we'll get an exception
               setAttribute(objectName, new Attribute(mbeanRefName, attribute));
            } // end of if ()
         }
         //end of depends
         else if (element.getTagName().equals("depends-list"))
         {
            String dependsListName = element.getAttribute("optional-attribute-name");
            if ("".equals(dependsListName))
            {
               dependsListName = null;
            } // end of if ()

            NodeList dependsList = element.getChildNodes();
            ArrayList dependsListNames = new ArrayList();
            for (int l = 0; l < dependsList.getLength(); l++)
            {
               if (dependsList.item(l).getNodeType() != Node.ELEMENT_NODE)
               {
                  continue;
               }

               Element dependsElement = (Element)dependsList.item(l);
               if (dependsElement.getTagName().equals("depends-list-element"))
               {
                  if (!dependsElement.hasChildNodes())
                  {
                     throw new DeploymentException("Empty depends-list-element!");
                  } // end of if ()

                  // Get the depends value
                  ObjectName dependsObjectName = processDependency(objectName, loaderName, dependsElement, mbeans);
                  if (!dependsListNames.contains(dependsObjectName))
                  {
                     dependsListNames.add(dependsObjectName);
                  } // end of if ()
               }

            } // end of for ()
            if (dependsListName != null)
            {
               setAttribute(objectName, new Attribute(dependsListName, dependsListNames));
            } // end of if ()
         }//end of depends-list
      }

      // Check for overriden attributes controlled by the service binding manager
      try
      {
         // Hard coded for now. We need someplace for standard service names...
         ObjectName serviceBindingMgr = new ObjectName("jboss.system:service=ServiceBindingManager");
         Object[] args = {objectName};
         String[] sig = {"javax.management.ObjectName"};
         server.invoke(serviceBindingMgr, "applyServiceConfig", args, sig);
      }
      catch(InstanceNotFoundException ignore)
      {
         // If there is no service binding manager do nothing
      }
      catch(Exception e)
      {
         // Log a debug message indicating a config was found but failed
         Throwable t = JMXExceptionDecoder.decode(e);
         log.warn("Failed to apply service binding override", t);
      }
   }

   private ObjectName processDependency(ObjectName container, ObjectName loaderName,
      Element element, List mbeans)
      throws Exception
   {
      ObjectName dependsObjectName = null;
      NodeList nl = element.getChildNodes();
      for (int i = 0; i < nl.getLength(); i++)
      {
         Node childNode = nl.item(i);
         if (childNode.getNodeType() == Node.ELEMENT_NODE)
         {
            Element child = (Element)childNode;
            if (child.getTagName().equals("mbean"))
            {
               dependsObjectName = internalInstall(child, mbeans, loaderName);
               break;
            }
            else
            {
               throw new DeploymentException("Non mbean child element in depends tag: " + child);
            } // end of else
         } // end of if ()
      } // end of for ()

      if (dependsObjectName == null)
      {
         String name = getElementContent(element, true);
         dependsObjectName = ObjectNameFactory.create(name);
      }
      if (dependsObjectName == null)
      {
         throw new DeploymentException("No object name found for attribute!");
      } // end of if ()

      serviceController.registerDependency(container, dependsObjectName);

      return dependsObjectName;
   }

   /** A helper to deal with those pesky JMX exceptions. */
   private void setAttribute(ObjectName name, Attribute attr)
      throws Exception
   {
      try
      {
         server.setAttribute(name, attr);
      }
      catch (Exception e)
      {
         throw new DeploymentException("Exception setting attribute " +
            attr + " on mbean " + name, JMXExceptionDecoder.decode(e));
      }
   }

   /**
    * Builds a string that consists of the configuration elements of
    * the currently running MBeans registered in the server.
    *
    * @throws Exception    Failed to construct configuration.
    *
    * @todo replace with more sophisticated mbean persistence mechanism.
    */
   public String getConfiguration(ObjectName[] objectNames)
      throws Exception
   {
      Writer out = new StringWriter();

      DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
      DocumentBuilder builder = factory.newDocumentBuilder();
      Document doc = builder.newDocument();

      Element serverElement = doc.createElement("server");

      // Store attributes as XML
      for (int j = 0 ; j<objectNames.length ; j++)
      {
         Element mbeanElement = internalGetConfiguration(doc, objectNames[j]);
         serverElement.appendChild(mbeanElement);
      }

      doc.appendChild(serverElement);

      // Write configuration
      (new DOMWriter(out, false)).print(doc, true);

      out.close();

      // Return configuration
      return out.toString();
   }

   Element getConfiguration(ObjectName name) throws Exception
   {
      DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
      DocumentBuilder builder = factory.newDocumentBuilder();
      Document doc = builder.newDocument();
      return internalGetConfiguration(doc, name);
   }

   private Element internalGetConfiguration(Document doc, ObjectName name)
      throws Exception
   {
      Element mbeanElement = doc.createElement("mbean");
      mbeanElement.setAttribute("name", name.toString());

      MBeanInfo info = server.getMBeanInfo(name);
      mbeanElement.setAttribute("code", info.getClassName());
      MBeanAttributeInfo[] attributes = info.getAttributes();
      boolean trace = log.isTraceEnabled();
      for (int i = 0; i < attributes.length; i++)
      {
         if( trace )
            log.trace("considering attribute: " + attributes[i]);
         if (attributes[i].isReadable() && attributes[i].isWritable())
         {
            Element attributeElement = null;
            if (attributes[i].getType().equals("javax.management.ObjectName"))
            {
               attributeElement = doc.createElement("depends");
               attributeElement.setAttribute("optional-attribute-name", attributes[i].getName());
            }
            else
            {
               attributeElement = doc.createElement("attribute");
               attributeElement.setAttribute("name", attributes[i].getName());
            }
            Object value = server.getAttribute(name, attributes[i].getName());

            if (value != null)
            {
               if (value instanceof Element)
               {
                  attributeElement.appendChild(doc.importNode((Element)value, true));
               }
               else
               {
                  attributeElement.appendChild(doc.createTextNode(value.toString()));
               }
            }
            mbeanElement.appendChild(attributeElement);
         }
      }

      ServiceContext sc = serviceController.getServiceContext(name);
      for (Iterator i = sc.iDependOn.iterator(); i.hasNext(); )
      {
         ServiceContext needs = (ServiceContext)i.next();
         Element dependsElement = doc.createElement("depends");
         dependsElement.appendChild(doc.createTextNode(needs.objectName.toString()));
         mbeanElement.appendChild(dependsElement);
      }

      return mbeanElement;
   }

   /**
    * Parse an object name from the given element attribute 'name'.
    *
    * @param element    Element to parse name from.
    * @return           Object name.
    *
    * @throws ConfigurationException   Missing attribute 'name'
    *                                  (thrown if 'name' is null or "").
    * @throws MalformedObjectNameException
    */
   private ObjectName parseObjectName(final Element element)
      throws Exception
   {
      String name = element.getAttribute("name");

      if (name == null || name.trim().equals(""))
      {
         throw new DeploymentException("MBean attribute 'name' must be given.");
      }

      return new ObjectName(name);
   }

   private String getElementContent(Element element, boolean trim)
      throws Exception
   {
      NodeList nl = element.getChildNodes();
      String attributeText = "";
      for (int i = 0; i < nl.getLength(); i++)
      {
         Node n = nl.item(i);
         if( n instanceof Text )
         {
            attributeText += ((Text)n).getData();
         }
      } // end of for ()
      if( trim )
         attributeText = attributeText.trim();
      return attributeText;
   }

}
