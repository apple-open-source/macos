/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.platform;

import org.jboss.util.ThrowableHandler;

/**
 * Provides common access to specifics about the version of <em>Java</em>
 * that a virtual machine supports.
 *
 * <p>Determines the version of the <em>Java Virtual Machine</em> by checking
 *    for the availablity of version specific classes.<p>
 *
 * <p>Classes are loaded in the following order:
 *    <ol>
 *    <li><tt>java.lang.StackTraceElement</tt> was introduced in JDK 1.4</li>
 *    <li><tt>java.lang.StrictMath</tt> was introduced in JDK 1.3</li>
 *    <li><tt>java.lang.ThreadLocal</tt> was introduced in JDK 1.2</li>
 *    <li><tt>java.lang.Void</tt> was introduced in JDK 1.1</li>
 *    </ol>
 * </p>
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class Java
{
   /** Prevent instantiation */
   private Java() {}

   /** Java version 1.0 token */
   public static final int VERSION_1_0 = 0x01;

   /** Java version 1.1 token */
   public static final int VERSION_1_1 = 0x02;

   /** Java version 1.2 token */
   public static final int VERSION_1_2 = 0x03;

   /** Java version 1.3 token */
   public static final int VERSION_1_3 = 0x04;

   /** Java version 1.4 token */
   public static final int VERSION_1_4 = 0x05;
   
   /** 
    * Private to avoid over optimization by the compiler.
    *
    * @see #getVersion()   Use this method to access this final value.
    */
   private static final int VERSION;

   /** Initialize VERSION. */ 
   static {
      // default to 1.0
      int version = VERSION_1_0;

      try {
         // check for 1.1
         Class.forName("java.lang.Void");
         version = VERSION_1_1;

         // check for 1.2
         Class.forName("java.lang.ThreadLocal");
         version = VERSION_1_2;

         // check for 1.3
         Class.forName("java.lang.StrictMath");
         version = VERSION_1_3;

         // check for 1.4
         Class.forName("java.lang.StackTraceElement");
         version = VERSION_1_4;
      }
      catch (ClassNotFoundException e) {
         ThrowableHandler.add(e);
      }
      
      VERSION = version;
   }

   /**
    * Return the version of <em>Java</em> supported by the VM.
    *
    * @return  The version of <em>Java</em> supported by the VM.
    */
   public static int getVersion() {
      return VERSION;
   }

   /**
    * Retrurns true if the given version identifer is equal to the
    * version identifier of the current virtuial machine.
    *
    * @param version    The version identifier to check for.
    * @return           True if the current virtual machine is the same version.
    */
   public static boolean isVersion(final int version) {
      return VERSION == version;
   }

   /**
    * Retrurns true if the current virtual machine is compatible with
    * the given version identifer.
    *
    * @param version    The version identifier to check compatibility of.
    * @return           True if the current virtual machine is compatible.
    */
   public static boolean isCompatible(final int version) {
      // if our vm is the same or newer then we are compatible
      return VERSION >= version;
   }
}
