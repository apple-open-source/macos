/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.compiler;

import org.apache.bcel.Constants;
import org.apache.bcel.generic.BasicType;
import org.apache.bcel.generic.ObjectType;
import org.apache.bcel.generic.Type;

import org.jboss.util.UnreachableStatementException;

/**
 * Some Routines to convert from <code>java.lang.Class</code> to
 * <code>org.apache.bcel.generic.Type</code>. These get round some
 * inconsistencies with Class.getName() wrt primitives.
 *
 * <pre>
 * e.g. 
 *
 *  <code>Character.Type.getName()</code> returns char.
 *
 * </pre>
 *
 * <p>
 * I think it should return C. Don't know if this is a code bug. But there's a bug
 * on Bug Parade (#4369208) about the javadoc being misleading.
 *
 * @see java.lang.Class#getName
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <a href="mailto:neale@isismanor.co.uk">Neale Swinnerton</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class Utility 
   extends org.apache.bcel.classfile.Utility 
{
   /**
    * Get the <code>org.apache.bcel.generic.Type</code> for a class.
    * This handles the case where the class represents an n-dimensional
    * array by relying on the fact that <code>Class.getName()</code>
    * on an array returns the signature
    *
    * <pre>
    * e.g.
    *
    *   <code>new Object[].getClass().getName()</code> returns [Ljava.lang.Object;
    *
    * </pre>
    *
    * @see Utility
    *
    * @param clazz    a <code>Class</code> value
    * @return         a <code>Type</code> value
    */
   public static Type getType(Class clazz) {
      if (clazz.isPrimitive()) {
         if (clazz.equals(Boolean.TYPE) ) {
            return Type.BOOLEAN;
         } 
         else if (clazz.equals(Byte.TYPE) ) {
            return Type.BYTE;
         } 
         else if (clazz.equals(Character.TYPE) ) {
            return Type.CHAR;
         } 
         else if (clazz.equals(Double.TYPE) ) {
            return Type.DOUBLE;
         } 
         else if (clazz.equals(Float.TYPE) ) {
            return Type.FLOAT;
         } 
         else if (clazz.equals(Integer.TYPE) ) {
            return Type.INT;
         } 
         else if (clazz.equals(Long.TYPE) ) {
            return Type.LONG;
         } 
         else if (clazz.equals(Short.TYPE) ) {
            return Type.SHORT;
         } 
         else if (clazz.equals(Void.TYPE) ) {
            return Type.VOID;
         }

         // should never get here
         throw new UnreachableStatementException();
      } 

      // if we get this far it is not a primitive
      String name = clazz.getName();

      if (clazz.isArray()) {
         return Type.getType(name);
      } 
      
      return new ObjectType(name);
   }
   
   /**
    * Get the <code>org.apache.bcel.generic.Type</code> for an array of Classes
    *
    * @param classes    a <code>Class[]</code> value
    * @return           a <code>Type[]</code> value
    */
   public static Type[] getTypes(Class[] classes) {
      Type[] types = new Type[classes.length];

      for (int i = 0; i < classes.length; i++) {
         types[i] = getType(classes[i]);
      }

      return types;
   }      

   /**
    * Get the Object equivalent Class name for a primitive
    *
    * <pre>
    * e.g
    *
    *   int   <-> java.lang.Integer
    *   char  <-> Character
    *
    * </pre>
    *
    * @param t    a <code>BasicType</code> value
    * @return     a <code>String</code> value
    *
    * @throws IllegalArgumentException   Unexpected type
    */
   public static String getObjectEquivalentClassName(BasicType t) {
      switch (t.getType()) {
      case Constants.T_INT:
         return "java.lang.Integer";

      case Constants.T_SHORT:
         return "java.lang.Short";

      case Constants.T_BOOLEAN:
         return "java.lang.Boolean";

      case Constants.T_CHAR:
         return "java.lang.Character";

      case Constants.T_BYTE:
         return "java.lang.Byte";

      case Constants.T_FLOAT:
         return "java.lang.Float";

      case Constants.T_DOUBLE:
         return "java.lang.Double";

      case Constants.T_LONG:
         return "java.lang.Long";

      default:
         throw new IllegalArgumentException("Unexpected Type: " + t);
      }
   }
      
}
