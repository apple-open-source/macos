/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.compiler;

import java.lang.reflect.Method;

import org.apache.bcel.Constants;
import org.apache.bcel.classfile.JavaClass;
import org.apache.bcel.generic.ClassGen;
import org.apache.bcel.generic.BasicType;
import org.apache.bcel.generic.Type;

import org.jboss.logging.Logger;

/**
 * Manages bytecode assembly for dynamic proxy generation.
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author Unknown
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ProxyCompiler
{
   /** Class logger. */
   private static final Logger log = Logger.getLogger(ProxyCompiler.class);

   /** The path (if non-null) where generated classes will be dumped for debugging. */
   public final static String CLASS_DUMP_PATH = 
      System.getProperty(ProxyCompiler.class.getName() + ".dumpPath", null);

   /** The suffix for proxy implementation classnames. */
   public final static String IMPL_SUFFIX = "$Proxy";

   /** The Runtime classloader for the target proxy. */
   Runtime runtime;

   /** The superclass of the target proxy. */
   Class superclass;

   /** The implementing types of the target proxy. */
   Class targetTypes[];

   /** The implementing methods of the target proxy. */
   Method methods[];

   /** The class of the targret proxy (set by runtime). */
   Class proxyType;
   
   /**
    * Creates a new <code>ProxyCompiler</code> instance.
    *
    * @param parent        a <code>ClassLoader</code> value
    * @param superclass    a <code>Class</code> value
    * @param targetTypes   a <code>Class</code> value
    * @param methods       a <code>Method</code> value
    */
   public ProxyCompiler(final ClassLoader parent,
                        final Class superclass,
                        final Class targetTypes[],
                        final Method methods[])
      throws Exception
   {
      this.superclass = superclass;
      this.targetTypes = targetTypes;
      this.methods = methods;

      this.runtime = new Runtime(parent);
      this.runtime.targetTypes = targetTypes;
      this.runtime.methods = methods;

      runtime.makeProxyType(this);
   }

   public Class getProxyType() {
      return proxyType;
   }
   
   public String getProxyClassName() {
      // Note:  We could reasonably put the $Impl class in either
      // of two packges:  The package of Proxies, or the same package
      // as the target type.  We choose to put it in same package as
      // the target type, to avoid name encoding issues.
      //
      // Note that all infrastructure must be public, because the
      // $Impl class is inside a different class loader.

      return targetTypes[0].getName() + IMPL_SUFFIX;
   }
      
   /**
    * Create the implementation class for the given target.
    *
    * @return a <code>byte[]</code> value
    */
   public byte[] getCode() 
   {
      boolean trace = log.isTraceEnabled();

      final String proxyClassName = getProxyClassName();
      final String superClassName = superclass.getName();

      int icount = 1; // don't forget ProxyTarget
      for (int i = 0; i < targetTypes.length; i++) {
         Class targetType = targetTypes[i];
         if (targetType.isInterface()) {
            icount++;
         }
      }

      String interfaceNames[] = new String[icount];
      interfaceNames[0] = Proxies.ProxyTarget.class.getName();
      icount = 1;
      for (int i = 0; i < targetTypes.length; i++) {
         Class targetType = targetTypes[i];
         if (targetType.isInterface()) {
            interfaceNames[icount++] = targetType.getName();
         } 
         else if (!superclass.isAssignableFrom(targetType)) {
            throw new RuntimeException("unexpected: " + targetType);
         }
      }

      ClassGen cg = new ClassGen(proxyClassName,
                                 superClassName,
                                 "<generated>",
                                 Constants.ACC_PUBLIC | Constants.ACC_FINAL,
				 interfaceNames);
      
      ProxyImplementationFactory factory = 
         new ProxyImplementationFactory(superClassName, proxyClassName, cg);

      cg.addField(factory.createInvocationHandlerField());
      cg.addField(factory.createRuntimeField());
      cg.addMethod(factory.createConstructor());
      
      // ProxyTarget implementation

      cg.addMethod(factory.createGetInvocationHandler());
      cg.addMethod(factory.createGetTargetTypes());
           
      boolean haveToString = false;

      if (trace) log.trace("Creating proxy methods...");

      // Implement the methods of the target types.
      for (int i = 0; i < methods.length; i++) 
      {
         Method m = methods[i];
         if (trace) log.trace("Reflected method: " + m);

         String name = m.getName();
         Class rTypeClass = m.getReturnType();
         String rTypeName = rTypeClass.getName();
         Type rType = Utility.getType(rTypeClass);
         Type[] pTypes = Utility.getTypes(m.getParameterTypes());
         String[] exceptionNames = getNames(m.getExceptionTypes());

         if (name.equals("toString") && pTypes.length == 0) {
            haveToString = true;
         }

         org.apache.bcel.classfile.Method proxyMethod = 
            factory.createProxyMethod(name, i, rType, pTypes, exceptionNames);

         if (trace) log.trace("Created proxy method: " + proxyMethod);

         cg.addMethod(proxyMethod);
      }

      if (!haveToString) {
         cg.addMethod(factory.createToString());
      }

      JavaClass jclass = cg.getJavaClass();
      if (trace) log.trace("Generated Java class: " + jclass);

      // dump the class if we have been configured todo so
      if (CLASS_DUMP_PATH != null) {
         try {
            String filename = CLASS_DUMP_PATH + java.io.File.separator + proxyClassName + ".class";
            log.info("Dumping generated proxy class to " + filename);
            jclass.dump(filename);
         } 
         catch (Exception e) {
            log.error("Failed to dump class file", e);
         }
      }

      return jclass.getBytes();
   }
   
   /**
    * Returns an array of class names for the given array
    * of classes.
    */
   private String[] getNames(Class[] classes) {
      String[] names = new String[classes.length];
      for ( int i = 0;  i < classes.length;  i++ ) {
         names[i] = classes[i].getName();
      }

      return names;
   }
}
