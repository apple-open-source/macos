/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.iiop;

import org.jboss.logging.Logger;
import org.jboss.proxy.compiler.IIOPStubCompiler;

/**
 * This class loader dynamically generates and loads client stub classes.
 * It is intended to be used by clients, as an interim solution.
 * Should not be necessary when the IORs contain a JAVA_CODE_BASE tag.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class StubClassLoader 
      extends ClassLoader
{

   // Static ------------------------------------------------------------------

   private static final Logger logger = 
                          Logger.getLogger(StubClassLoader.class);

   // Constructor -------------------------------------------------------------
   
   public StubClassLoader(ClassLoader parent)
   {
      super(parent);
   }

   // Protected ---------------------------------------------------------------

   protected  Class findClass(String name) 
         throws ClassNotFoundException 
   {
      logger.debug("findClass(" + name + ") called");
      if (name.endsWith("_Stub")) {
         int start = name.lastIndexOf('.') + 1;
         if (name.charAt(start) == '_') {
            String pkg = name.substring(0, start);
            String interfaceName = pkg + name.substring(start + 1, 
                                                        name.length() - 5);
            logger.debug("interface name " + interfaceName);
            Class intf = loadClass(interfaceName);
            logger.debug("loaded class " + interfaceName);

            try {
               byte[] code = IIOPStubCompiler.compile(intf, name);
               
               logger.debug("compiled stub class for " + interfaceName);
               Class clz = defineClass(name, code, 0, code.length);
               logger.debug("defined stub class for " + interfaceName);
               resolveClass(clz);
               logger.debug("resolved stub class for " + interfaceName);
               return clz;
            }
            catch (RuntimeException e) {
               logger.debug("Exception generating IIOP stub " + name, e);
               throw e;
            }
         }
      }
      throw new ClassNotFoundException(name);
   }

}
