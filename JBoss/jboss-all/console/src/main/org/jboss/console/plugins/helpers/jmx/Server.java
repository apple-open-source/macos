package org.jboss.console.plugins.helpers.jmx;

import org.jboss.logging.Logger;
import org.jboss.mx.util.MBeanServerLocator;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.IntrospectionException;
import javax.management.JMException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;
import java.util.TreeMap;

/** Utility methods related to the MBeanServer interface
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.4 $
 */
public class Server
{
   static Logger log = Logger.getLogger(Server.class);

   public static MBeanServer getMBeanServer() throws JMException
   {
      return MBeanServerLocator.locateJBoss();
   }

   public static Iterator getDomainData(String filter) throws JMException
   {
      MBeanServer server = getMBeanServer();
      TreeMap domainData = new TreeMap();
      if( server != null )
      {
         ObjectName filterName = null;
         if( filter != null )
            filterName = new ObjectName(filter);
         Set objectNames = server.queryNames(filterName, null);
         Iterator objectNamesIter = objectNames.iterator();
         while( objectNamesIter.hasNext() )
         {
            ObjectName name = (ObjectName) objectNamesIter.next();
            MBeanInfo info = server.getMBeanInfo(name);
            String domainName = name.getDomain();
            MBeanData mbeanData = new MBeanData(name, info);
            DomainData data = (DomainData) domainData.get(domainName);
            if( data == null )
            {
               data = new DomainData(domainName);
               domainData.put(domainName, data);
            }
            data.addData(mbeanData);
         }
      }
      Iterator domainDataIter = domainData.values().iterator();
      return domainDataIter;
   }

   public static MBeanData getMBeanData(String name) throws JMException
   {
      MBeanServer server = getMBeanServer();
      ObjectName objName = new ObjectName(name);
      MBeanInfo info = server.getMBeanInfo(objName);
      MBeanData mbeanData = new MBeanData(objName, info);
      return mbeanData;
   }

   public static Object getMBeanAttributeObject(String name, String attrName)
      throws JMException
   {
      MBeanServer server = getMBeanServer();
      ObjectName objName = new ObjectName(name);
      Object value = server.getAttribute(objName, attrName);
      return value;
   }

   public static String getMBeanAttribute(String name, String attrName) throws JMException
   {
      MBeanServer server = getMBeanServer();
      ObjectName objName = new ObjectName(name);
      String value = null;
      try
      {
         Object attr = server.getAttribute(objName, attrName);
         if( attr != null )
            value = attr.toString();
      }
      catch(JMException e)
      {
         value = e.getMessage();
      }
      return value;
   }

   public static AttrResultInfo getMBeanAttributeResultInfo(String name, MBeanAttributeInfo attrInfo)
      throws JMException
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      MBeanServer server = getMBeanServer();
      ObjectName objName = new ObjectName(name);
      String attrName = attrInfo.getName();
      String attrType = attrInfo.getType();
      Object value = null;
      Throwable throwable = null;
      if( attrInfo.isReadable() == true )
      {
         try
         {
            value = server.getAttribute(objName, attrName);
         }
         catch (Throwable t)
         {
            throwable = t;
         }
      }
      Class typeClass = null;
      try
      {
         typeClass = getPrimativeClass(attrType);
         if( typeClass == null )
            typeClass = loader.loadClass(attrType);
      }
      catch(ClassNotFoundException ignore)
      {
      }
      PropertyEditor editor = null;
      if( typeClass != null )
         editor = PropertyEditorManager.findEditor(typeClass);

      return new AttrResultInfo(attrName, editor, value, throwable);
   }

   public static AttributeList setAttributes(String name, HashMap attributes) throws JMException
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      MBeanServer server = getMBeanServer();
      ObjectName objName = new ObjectName(name);
      MBeanInfo info = server.getMBeanInfo(objName);
      MBeanAttributeInfo[] attributesInfo = info.getAttributes();
      AttributeList newAttributes = new AttributeList();
      for(int a = 0; a < attributesInfo.length; a ++)
      {
         MBeanAttributeInfo attrInfo = attributesInfo[a];
         String attrName = attrInfo.getName();
         if( attributes.containsKey(attrName) == false )
            continue;
         String value = (String) attributes.get(attrName);
         String attrType = attrInfo.getType();
         Attribute attr = null;
         try
         {
            Class argType = getPrimativeClass(attrType);
            if( argType == null )
               argType = loader.loadClass(attrType);
            PropertyEditor editor = PropertyEditorManager.findEditor(argType);
            // Ignore attributes without valid editors
            if( editor == null )
            {
               log.trace("Failed to find PropertyEditor for type: "+attrType);
               continue;
            }
            editor.setAsText(value);
            attr = new Attribute(attrName, editor.getValue());
         }
         catch(ClassNotFoundException e)
         {
            log.trace("Failed to load class for attribute: "+ (attr==null?"null":attr.getName()), e);
            throw new ReflectionException(e, "Failed to load class for attribute: " + (attr==null?"null":attr.getName()));
         }

         server.setAttribute(objName, attr);
         newAttributes.add(attr);
      }
      return newAttributes;
   }

   public static OpResultInfo invokeOp(String name, int index, String[] args) throws JMException
   {
      MBeanServer server = getMBeanServer();
      ObjectName objName = new ObjectName(name);
      MBeanInfo info = server.getMBeanInfo(objName);
      MBeanOperationInfo[] opInfo = info.getOperations();
      MBeanOperationInfo op = opInfo[index];
      MBeanParameterInfo[] paramInfo = op.getSignature();
      String[] argTypes = new String[paramInfo.length];
      for(int p = 0; p < paramInfo.length; p ++)
         argTypes[p] = paramInfo[p].getType();
      return invokeOpByName(name, op.getName(), argTypes, args);
   }

   public static OpResultInfo invokeOpByName(String name, String opName, String[] argTypes, String[] args)
      throws JMException
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      MBeanServer server = getMBeanServer();
      ObjectName objName = new ObjectName(name);
      int length = argTypes != null ? argTypes.length : 0;
      Object[] typedArgs = new Object[length];
      for(int p = 0; p < typedArgs.length; p ++)
      {
         String arg = args[p];
         try
         {
            Class argType = getPrimativeClass(argTypes[p]);
            if( argType == null )
               argType = loader.loadClass(argTypes[p]);
            PropertyEditor editor = PropertyEditorManager.findEditor(argType);
            if( editor == null )
               throw new IntrospectionException("Failed to find PropertyEditor for type: "+argTypes[p]);
            editor.setAsText(arg);
            typedArgs[p] = editor.getValue();
         }
         catch(ClassNotFoundException e)
         {
            log.trace("Failed to load class for arg"+p, e);
            throw new ReflectionException(e, "Failed to load class for arg"+p);
         }
      }
      Object opReturn = server.invoke(objName, opName, typedArgs, argTypes);
      return new OpResultInfo(opName, argTypes, opReturn);
   }

   static Class getPrimativeClass(String type)
   {
      String[] primatives = {
            "boolean", "byte", "char", "int", "short",
            "float", "double", "long"
         };
      Class[] primativeTypes = {
         boolean.class, byte.class, char.class, int.class, short.class,
         float.class, double.class, long.class
      };
      Class c = null;
      for(int p = 0; p < primatives.length; p ++)
      {
         if( type.equals(primatives[p]) )
            c = primativeTypes[p];
      }
      return c;
   }
}
