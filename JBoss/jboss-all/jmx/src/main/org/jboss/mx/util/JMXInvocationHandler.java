/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import java.io.Serializable;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

import java.util.HashMap;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.DynamicMBean;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeOperationsException;
import javax.management.RuntimeMBeanException;
import javax.management.RuntimeErrorException;

/**
 * <description> 
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3 $
 *   
 */
public class JMXInvocationHandler 
      implements ProxyContext, InvocationHandler, Serializable
{  
      
   // Attributes -------------------------------------------------
   protected MBeanServer server    = null;
   protected ObjectName objectName = null;
   
   private ProxyExceptionHandler handler = new DefaultExceptionHandler();
   private HashMap attributeMap          = new HashMap();
   
   // Constructors -----------------------------------------------
   public JMXInvocationHandler(MBeanServer server, ObjectName name) throws MBeanProxyCreationException
   {
      try
      {
         if (server == null)
            throw new MBeanProxyCreationException("null agent reference");
            
         this.server     = server;
         this.objectName = name;
         
         MBeanInfo info = server.getMBeanInfo(objectName);
         MBeanAttributeInfo[] attributes = info.getAttributes();

         for (int i = 0; i < attributes.length; ++i)
            attributeMap.put(attributes[i].getName(), attributes[i]);
      }
      catch (InstanceNotFoundException e)
      {
         throw new MBeanProxyCreationException("Object name " + name + " not found: " + e.toString());
      }
      catch (IntrospectionException e)
      {
         throw new MBeanProxyCreationException(e.toString());
      }
      catch (ReflectionException e)
      {
         throw new MBeanProxyCreationException(e.toString());
      }
   }
   
   // InvocationHandler implementation ---------------------------
   public Object invoke(Object proxy, Method method, Object[] args) throws Exception
   {
      Class declaringClass = method.getDeclaringClass();
      
      if (method.getDeclaringClass() == ProxyContext.class)
         return method.invoke(this, args);
      
      if (method.getDeclaringClass() == DynamicMBean.class)
      {
         String methodName = method.getName();
         
         if (methodName.equals("setAttribute"))
         {
            server.setAttribute(objectName, (Attribute)args[0]);
            return null;
         }
         else if (methodName.equals("setAttributes"))
            return server.setAttributes(objectName, (AttributeList)args[0]);
         else if (methodName.equals("getAttribute"))
            return server.getAttribute(objectName, (String)args[0]);
         else if (methodName.equals("getAttributes"))
            return server.getAttributes(objectName, (String[])args[0]);
         else if (methodName.equals("invoke"))
            return server.invoke(objectName, (String)args[0], (Object[])args[1], (String[])args[2]);
         else if (methodName.equals("getMBeanInfo"))
            return server.getMBeanInfo(objectName);
      }
      
      try 
      {
         String methodName = method.getName();
         
         if (methodName.startsWith("get") && args == null)
         {
            String attrName = methodName.substring(3, methodName.length());
            
            MBeanAttributeInfo info = (MBeanAttributeInfo)attributeMap.get(attrName);
            if (info != null)
            {
               String retType  = method.getReturnType().getName();
               
               if (retType.equals(info.getType())) 
               {
                  return server.getAttribute(objectName, attrName);
               }
            }
         }
         
         else if (methodName.startsWith("is") && args == null)
         {
            String attrName = methodName.substring(2, methodName.length());
            
            MBeanAttributeInfo info = (MBeanAttributeInfo)attributeMap.get(attrName);
            if (info != null && info.isIs())
            {
               Class retType = method.getReturnType();
               
               if (retType.equals(Boolean.class) || retType.equals(Boolean.TYPE))
               {
                  return server.getAttribute(objectName, attrName);
               }
            }
         }
         
         else if (methodName.startsWith("set") && args != null && args.length == 1)
         {
            String attrName = methodName.substring(3, methodName.length());
            
            MBeanAttributeInfo info = (MBeanAttributeInfo)attributeMap.get(attrName);
            if (info != null && method.getReturnType().equals(Void.TYPE))
            {
               if (info.getType().equals(args[0].getClass().getName()))
               {
                  server.setAttribute(objectName, new Attribute(attrName, args[0]));
                  return null;
               }
            }
         }

         String[] signature = null;
         
         if (args != null)
         {
            signature = new String[args.length];
            Class[] sign = method.getParameterTypes();
            
            for (int i = 0; i < sign.length; ++i)
               signature[i] = sign[i].getName();
         }
         
         return server.invoke(objectName, methodName, args, signature);
      }
      catch (InstanceNotFoundException e)
      {
         return getExceptionHandler().handleInstanceNotFound(this, e, method, args);
      }
      catch (AttributeNotFoundException e)
      {
         return getExceptionHandler().handleAttributeNotFound(this, e, method, args);
      }
      catch (InvalidAttributeValueException e)
      {
         return getExceptionHandler().handleInvalidAttributeValue(this, e, method, args);
      }
      catch (MBeanException e)
      {
         return getExceptionHandler().handleMBeanException(this, e, method, args);
      }
      catch (ReflectionException e)
      {
         return getExceptionHandler().handleReflectionException(this, e, method, args);
      }
      catch (RuntimeOperationsException e)
      {
         return getExceptionHandler().handleRuntimeOperationsException(this, e, method, args);
      }
      catch (RuntimeMBeanException e)
      {
         return getExceptionHandler().handleRuntimeMBeanException(this, e, method, args);
      }
      catch (RuntimeErrorException e)
      {
         return getExceptionHandler().handleRuntimeError(this, e, method, args);
      }
   }

   public ProxyExceptionHandler getExceptionHandler()
   {
      return handler;
   }
   
   
   // ProxyContext implementation -----------------------------------
   
   // The proxy provides an access point for the client to methods not part
   // of the MBean's management interface. It can be used to configure the
   // invocation (with context, client side interceptors, RPC), exception
   // handling, act as an access point to MBean server interface and so on.
      
   public void setExceptionHandler(ProxyExceptionHandler handler)
   {
      this.handler = handler;
   }
   
   public MBeanServer getMBeanServer() 
   {
      return server;
   }      

   public ObjectName getObjectName()
   {
      return objectName;
   }
   
}
      



