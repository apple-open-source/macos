/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.deployment;

import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.UndeclaredThrowableException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;

import javax.management.Attribute;
import javax.management.AttributeNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.system.ServiceDynamicMBeanSupport;

import bsh.Interpreter;
import bsh.EvalError;

/** A wrapper service that exposes a BeanShell script as a JBoss service
 * mbean.
 *
 * @see BeanShellSubDeployer
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.5 $
 */
public class BeanShellScript extends ServiceDynamicMBeanSupport
{

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   protected DeploymentInfo deploymentInfo = null;
   protected String name = null;

   protected Interpreter interpreter = null;
   protected ScriptService scriptService = null;

   protected ObjectName preferedObjectName = null;
   protected ObjectName[] dependsServices = null;
   protected HashMap supportedInterfaces = new HashMap ();

   protected MBeanInfo mbeanInfo = null;

   protected static HashMap basicTypes = new HashMap ();

   static
   {
      basicTypes.put(Boolean.class, boolean.class);
      basicTypes.put(Byte.class, byte.class);
      basicTypes.put(Character.class, char.class);
      basicTypes.put(Integer.class, int.class);
      basicTypes.put(Short.class, short.class);
      basicTypes.put(Float.class, float.class);
      basicTypes.put(Double.class, double.class);
      basicTypes.put(Long.class, long.class);
   }

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public BeanShellScript(final DeploymentInfo di)
      throws DeploymentException
   {
      try
      {
         this.deploymentInfo = di;
         String name = deploymentInfo.url.toString();
         if (name.endsWith("/"))
         {
            name = name.substring(0, name.length() - 1);
         }
         this.name = name;

         loadScript (di.url);
      }
      catch (Exception e)
      {
         throw new DeploymentException (e);
      }
   }

   // Public --------------------------------------------------------

   // Z implementation ----------------------------------------------

   // ServiceDynamicMBeanSupport overrides --------------------------

   protected Object getInternalAttribute(String attribute)
      throws AttributeNotFoundException, MBeanException, ReflectionException
   {
      try
      {
         String action = "get" + attribute.substring(0, 1).toUpperCase() + attribute.substring(1);

         InvocationCouple invoc = retrieveCompatibleInvocation
            (action, new Class[0]);
         if (invoc == null)
            throw new AttributeNotFoundException (attribute + " getter not implemented on target script");

         return invoc.method.invoke(invoc.proxy, null);

      }
      catch (ClassNotFoundException cnfe)
      {
         throw new javax.management.ReflectionException (cnfe, "A signature class couldn't be loaded");
      }
      catch (IllegalAccessException iae)
      {
         throw new javax.management.ReflectionException (iae, "Problem while invoking gettter for field " + attribute);
      }
      catch (InvocationTargetException ite)
      {
         throw new MBeanException (ite, "Problem while invoking gettter for field " + attribute);
      }
  }

   protected void setInternalAttribute(Attribute attribute)
      throws
         AttributeNotFoundException,
         InvalidAttributeValueException,
         MBeanException,
         ReflectionException
   {
      String field = attribute.getName();
      try
      {
         String action = "set" + field.substring(0, 1).toUpperCase() + field.substring(1);
         Object value = attribute.getValue();
         Class clazz = value.getClass();
         Class tmp = (Class)basicTypes.get(clazz);
         if (tmp != null)
            clazz = tmp;

         InvocationCouple invoc = retrieveCompatibleInvocation
            (action, new Class[] {clazz});
         if (invoc == null)
            throw new AttributeNotFoundException (field + " setter not implemented on target script");

         invoc.method.invoke(invoc.proxy, new Object[] {value});

      }
      catch (ClassNotFoundException cnfe)
      {
         throw new javax.management.ReflectionException (cnfe, "A signature class couldn't be loaded");
      }
      catch (IllegalAccessException iae)
      {
         throw new javax.management.ReflectionException (iae, "Problem while invoking setter for field " + field);
      }
      catch (InvocationTargetException ite)
      {
         throw new MBeanException (ite, "Problem while invoking setter for field " + field);
      }
   }

   protected Object internalInvoke(String actionName, Object[] params, String[] signature)
      throws MBeanException, ReflectionException
   {
      try
      {
         InvocationCouple invoc = retrieveCompatibleInvocation (actionName, signature);
         if (invoc == null)
            throw new javax.management.ReflectionException (new Exception(), actionName + " not implemented on target script");

         Object value = invoc.method.invoke(invoc.proxy, params);
         return value;

      }
      catch (ClassNotFoundException cnfe)
      {
         throw new javax.management.ReflectionException (cnfe, "A signature class couldn't be loaded");
      }
      catch (IllegalAccessException iae)
      {
         throw new javax.management.ReflectionException (iae, "Problem while invoking " + actionName);
      }
      catch (InvocationTargetException ite)
      {
         throw new MBeanException (ite, "Problem while invoking " + actionName);
      }
   }

   public MBeanInfo getMBeanInfo()
   {
      return this.mbeanInfo;
   }

   // ServiceMBeanSupport overrides ---------------------------------------------------

   protected void createService() throws Exception
   {
      try
      {
         this.scriptService.setCtx(this);
      }
      catch (UndeclaredThrowableException e)
      {
         log.trace("UndeclaredThrowableException during setCtx", e);
      }
      catch (Throwable t)
      {
         log.warn("Error during setCtx", t);
      }

      try
      {
         this.scriptService.create();
      }
      catch (UndeclaredThrowableException e)
      {
         log.trace("UndeclaredThrowableException during create", e);
      }
      catch (EvalError e)
      {
         log.debug("EvalError during create", e);
      }
      catch (Exception e)
      {
         log.error("Error during create", e);
         throw e;
      }
   }

   protected void startService() throws Exception
   {
      try
      {
         this.scriptService.start();
      }
      catch (UndeclaredThrowableException e)
      {
         log.trace("UndeclaredThrowableException during start", e);
      }
      catch(EvalError e)
      {
         log.debug("EvalError during start", e);
      }
      catch (Exception e)
      {
         log.error("Error during start", e);
         throw e;
      }
   }

   protected void stopService() throws Exception
   {
      try
      {
         this.scriptService.stop();
      }
      catch (UndeclaredThrowableException e)
      {
         log.trace("UndeclaredThrowableException during stop", e);
      }
      catch (Exception e)
      {
         log.debug("Error during stop", e);
         throw e;
      }
   }

   protected void destroyService() throws Exception
   {
      try
      {
         this.scriptService.destroy();
      }
      catch (UndeclaredThrowableException e)
      {
         log.trace("UndeclaredThrowableException during destroy", e);
      }
      catch (Exception e)
      {
         log.debug("Error during destroy", e);
         throw e;
      }
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   protected InvocationCouple retrieveCompatibleInvocation
      (String name, String[] signature)
      throws ClassNotFoundException
   {
      ClassLoader ucl = Thread.currentThread().getContextClassLoader();
      // first transform signature
      //
      Class[] realSignature = null;
      if (signature != null)
      {
         realSignature = new Class[signature.length];
         for (int i=0; i<signature.length;i++)
            realSignature[i] = ucl.loadClass(signature[i]);
      }

      return retrieveCompatibleInvocation (name, realSignature);
   }

   protected InvocationCouple retrieveCompatibleInvocation
      (String name, Class[] signature)
      throws ClassNotFoundException
   {
      Iterator keys = supportedInterfaces.keySet().iterator();
      while (keys.hasNext())
      {
         Class key = (Class)keys.next();
         try
         {
            Method method = key.getMethod(name, signature);

            Object targetProxy = supportedInterfaces.get(key);
            return new InvocationCouple (targetProxy, method);
         }
         catch (NoSuchMethodException ok) {}
      }

      // if we arrive here it means that this operation does not exist!
      //
      return null;


   }

   protected void loadScript (java.net.URL url) throws Exception
   {
      InputStream stream = null;
      try
      {
         interpreter = new Interpreter ();
         interpreter.setClassLoader(Thread.currentThread().getContextClassLoader());
         stream = url.openStream();
         interpreter.eval (new java.io.InputStreamReader (stream));
      }
      finally
      {
         if (stream != null)
         {
            try
            {
               stream.close();
            }
            catch (Exception strange) { log.info(strange); }
         }

      }

      scriptService = (ScriptService)interpreter.getInterface(ScriptService.class);

      // We now load the script preferences
      //
      try
      {
         String[] depends = scriptService.dependsOn ();
         if (depends != null && depends.length > 0)
         {
            dependsServices = new ObjectName[depends.length];
            for (int i=0; i<depends.length; i++)
               dependsServices[i] = new ObjectName (depends[i]);
         }
      }
      catch (UndeclaredThrowableException ignored) {}

      try
      {
         String myName = scriptService.objectName ();
         if (myName != null)
            this.preferedObjectName = new ObjectName (myName);
      }
      catch (UndeclaredThrowableException ignored) {}

      try
      {
         Class[] intfs = scriptService.getInterfaces ();
         if (intfs != null && intfs.length > 0)
         {
            for (int i=0; i<intfs.length; i++)
            {
               Object iface = interpreter.getInterface(intfs[i]);
               supportedInterfaces.put (intfs[i], iface);
            }
         }

         this.mbeanInfo = generateMBeanInfo (intfs);
      }
      catch (UndeclaredThrowableException ignored)
      {
         log.debug("Failed to obtain interfaces", ignored);
         this.mbeanInfo = generateMBeanInfo (null);
      }

   }

   protected MBeanInfo generateMBeanInfo (Class[] intfs)
      throws IntrospectionException
   {
      MBeanInfo result = super.getMBeanInfo();

      if (intfs != null && intfs.length > 0)
      {
         ArrayList attrs = new ArrayList (Arrays.asList(result.getAttributes()));
         ArrayList ops = new ArrayList (Arrays.asList(result.getOperations()));

         HashMap readAttr = new HashMap ();
         HashMap writeAttr = new HashMap ();

         //  we now populate the MBeanInfo with information from our script
         //
         for (int i=0; i<intfs.length; i++)
         {
            Class clazz = intfs[i];
            Method[] methods = clazz.getMethods();
            for (int m=0; m<methods.length; m++)
            {
               Method meth = methods[m];
               String name = meth.getName();
               Class[] params = meth.getParameterTypes();

               if (name.startsWith("get") && params.length == 0)
               {
                  readAttr.put (name, meth);
               }
               else if (name.startsWith("set") && params.length == 1)
               {
                  writeAttr.put (name, meth);
               }
               else
               {
                  ops.add(new MBeanOperationInfo
                     (
                     "Method " + name + " from class/interface " + clazz.getName(), meth
                     )
                  );

               }
            }
         }

         // we now combine the getters and setters in single RW attributes
         //
         Iterator readKeys = readAttr.keySet().iterator();
         while (readKeys.hasNext())
         {
            String getter = (String)readKeys.next();
            Method getterMethod = (Method)readAttr.get( getter );

            String attribute = getter.substring(3);
            String setter = "set" + attribute;

            Method setterMethod = (Method)writeAttr.remove(setter);
            attrs.add (new MBeanAttributeInfo (attribute, "", getterMethod, setterMethod));
         }

         // we  add the remaining WO attributes
         //
         Iterator writeKeys = writeAttr.keySet().iterator();
         while (writeKeys.hasNext())
         {
            String setter = (String)writeKeys.next();
            Method setterMethod = (Method)writeAttr.get( setter );
            String attribute = setter.substring(3);

            attrs.add (new MBeanAttributeInfo (attribute, "", null, setterMethod));
         }


         result = new MBeanInfo(this.name,
                           "Dynamic MBean Service around BSH script " + this.name,
                           (MBeanAttributeInfo[])attrs.toArray(new MBeanAttributeInfo[attrs.size()]),
                           result.getConstructors(),
                           (MBeanOperationInfo[])ops.toArray(new MBeanOperationInfo[ops.size()]),
                           result.getNotifications());
      }

      return result;
   }

   public ObjectName getPreferedObjectName ()
   {
      return this.preferedObjectName;
   }

   public ObjectName[] getDependsServices ()
   {
      return this.dependsServices;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

   public class InvocationCouple
   {
      public Object proxy = null;
      public Method method = null;

      public InvocationCouple (Object proxy, Method m)
      {
         this.proxy = proxy;
         this.method = m;
      }

   }
}
