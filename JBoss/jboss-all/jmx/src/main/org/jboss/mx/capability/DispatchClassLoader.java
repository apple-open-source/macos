/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.capability;

/**
 * <description> 
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class DispatchClassLoader extends ClassLoader
{

   // Attributes ----------------------------------------------------
   private String name = null;
   private byte[] code = null;
   
   // Constructors --------------------------------------------------
   DispatchClassLoader(ClassLoader parent, String name, byte[] bytecode)
   {
      super(parent);
      
      this.name = name;
      this.code = bytecode;
   }
   
   DispatchClassLoader(String name, byte[] bytecode)
   {
      super();
      
      this.name = name;
      this.code = bytecode;            
   }
   
   // Protected -----------------------------------------------------
   protected Class findClass(String name) throws ClassNotFoundException
   {
      if (!name.equals(this.name))
         throw new ClassNotFoundException("Class not found: " + name + "(I'm a dispatch loader, I only know " + this.name + ")");
         
      return defineClass(name, code, 0, code.length);

   }
}
      



