/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import org.omg.CORBA.Any;


/**
 *  Analysis class for primitive types.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public class PrimitiveAnalysis
   extends ClassAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   /**
    *  Get a singleton instance representing one of the peimitive types.
    */
   static public PrimitiveAnalysis getPrimitiveAnalysis(Class cls)
   {
      if (cls == null)
         throw new IllegalArgumentException("Null class");

      if (cls == Void.TYPE)
         return voidAnalysis;
      if (cls == Boolean.TYPE)
         return booleanAnalysis;
      if (cls == Character.TYPE)
         return charAnalysis;
      if (cls == Byte.TYPE)
         return byteAnalysis;
      if (cls == Short.TYPE)
         return shortAnalysis;
      if (cls == Integer.TYPE)
         return intAnalysis;
      if (cls == Long.TYPE)
         return longAnalysis;
      if (cls == Float.TYPE)
         return floatAnalysis;
      if (cls == Double.TYPE)
         return doubleAnalysis;

      throw new IllegalArgumentException("Not a primitive type: " +
                                         cls.getName());
   }

   public static PrimitiveAnalysis voidAnalysis = new PrimitiveAnalysis(Void.TYPE, "void", "void");
   public static PrimitiveAnalysis booleanAnalysis = new PrimitiveAnalysis(Boolean.TYPE, "boolean", "boolean");
   public static PrimitiveAnalysis charAnalysis = new PrimitiveAnalysis(Character.TYPE, "wchar", "char");
   public static PrimitiveAnalysis byteAnalysis = new PrimitiveAnalysis(Byte.TYPE, "octet", "byte");
   public static PrimitiveAnalysis shortAnalysis = new PrimitiveAnalysis(Short.TYPE, "short", "short");
   public static PrimitiveAnalysis intAnalysis = new PrimitiveAnalysis(Integer.TYPE, "long", "int");
   public static PrimitiveAnalysis longAnalysis = new PrimitiveAnalysis(Long.TYPE, "long_long", "long");
   public static PrimitiveAnalysis floatAnalysis = new PrimitiveAnalysis(Float.TYPE, "float", "float");
   public static PrimitiveAnalysis doubleAnalysis = new PrimitiveAnalysis(Double.TYPE, "double", "double");

   // Constructors --------------------------------------------------

   private PrimitiveAnalysis(Class cls, String idlName, String javaName)
   {
      super(cls, idlName, javaName);
   }

   // Public --------------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

}
