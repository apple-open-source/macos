/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.compiler;

import java.lang.reflect.Method;
import java.lang.reflect.Field;

import java.io.InputStream;

import java.net.URL;

import org.jboss.util.NestedRuntimeException;

/**
 * Manages bytecode assembly for dynamic proxy generation.
 *
 * <p>This is the only data needed at runtime.
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author Unknown
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Runtime
   extends ClassLoader
{
   /** The field name of the runtime target proxies Runtime object. */
   public final static String RUNTIME_FN = "runtime";
   
   /**
    * Construct a new <tt>Runtime</tt>
    *
    * @param parent    The parent classloader to delegate to.
    */
   public Runtime(ClassLoader parent)
   {
      super(parent);
   }

   // These members are common utilities used by ProxyTarget classes.
   // They are all public so they can be linked to from generated code.
   // I.e., they are the runtime support for the code compiled below.

   Class targetTypes[];
   Method methods[];
   ProxyCompiler compiler;// temporary!

   public Class[] copyTargetTypes() {
      return (Class[])targetTypes.clone();
   }

   public Object invoke(InvocationHandler invocationHandler, int methodNum, Object values[])
      throws Throwable 
   {
      return invocationHandler.invoke(null, methods[methodNum], values);
   }

   void makeProxyType(ProxyCompiler compiler) 
      throws Exception
   {
      this.compiler = compiler; // temporary, for use during loading
      byte code[] = compiler.getCode();

      compiler.proxyType = super.defineClass(compiler.getProxyClassName(), code, 0, code.length);
      super.resolveClass(compiler.proxyType);

      // set the Foo$Impl.info pointer to myself
      Field field = compiler.proxyType.getField(RUNTIME_FN);
      field.set(null, this);

      compiler = null;
   }

   ClassLoader getTargetClassLoader() 
   {
      return getParent();
   }

   public synchronized Class loadClass(String name, boolean resolve)
      throws ClassNotFoundException 
   {
      // isn't this redundant?
      if (name.endsWith("$Proxy") && name.equals(compiler.getProxyClassName())) {
         return compiler.proxyType;
      }

      // delegate to the original class loader
      ClassLoader cl = getTargetClassLoader();
      if (cl == null) {
         return super.findSystemClass(name);
      }

      return cl.loadClass(name);
   }

   /**
    * Delegate to the original class loader.
    */
   public InputStream getResourceAsStream(String name) 
   {
      ClassLoader cl = getTargetClassLoader();

      if (cl == null) {
         return super.getSystemResourceAsStream(name);
      }

      return cl.getResourceAsStream(name);
   }

   /**
    * Delegate to the original class loader.
    */
   public URL getResource(String name) 
   {
      ClassLoader cl = getTargetClassLoader();

      if (cl == null) {
         return super.getSystemResource(name);
      }

      return cl.getResource(name);
   }
}
