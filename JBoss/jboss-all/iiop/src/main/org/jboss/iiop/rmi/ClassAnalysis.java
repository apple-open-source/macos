/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import org.omg.CORBA.Any;


/**
 *  Analysis class for classes. These define IDL types.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public class ClassAnalysis
   extends AbstractAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   /**
    *  Analyze the given class, and return the analysis.
   public static ClassAnalysis getClassAnalysis(Class cls)
      throws RMIIIOPViolationException
   {
      if (cls == null)
         throw new IllegalArgumentException("Cannot analyze NULL class.");
      if (cls == java.lang.String.class || cls == java.lang.Object.class     ||
          cls == java.lang.Class.class  || cls == java.io.Serializable.class ||
          cls == java.io.Externalizable.class ||
          cls == java.rmi.Remote.class)
         throw new IllegalArgumentException("Cannot analyze special class: " +
                                            cls.getName());
 
      if (cls.isPrimitive())
         return PrimitiveAnalysis.getPrimitiveAnalysis(cls);


      if (cls.isInterface() && java.rmi.Remote.class.isAssignableFrom(cls))
         return InterfaceAnalysis.getInterfaceAnalysis(cls);
// TODO
throw new RuntimeException("ClassAnalysis.getClassAnalysis() TODO");
   }
    */

   static private String javaNameOfClass(Class cls)
   {
      if (cls == null)
         throw new IllegalArgumentException("Cannot analyze NULL class.");
 
      String s = cls.getName();
 
      return s.substring(s.lastIndexOf('.')+1);
   }

   // Constructors --------------------------------------------------

   public ClassAnalysis(Class cls, String idlName, String javaName)
   {
      super(idlName, javaName);

      this.cls = cls;
   }

   public ClassAnalysis(Class cls, String javaName)
   {
      this(cls, Util.javaToIDLName(javaName), javaName);
   }

   public ClassAnalysis(Class cls)
   {
      this(cls, javaNameOfClass(cls));
   }

   // Public --------------------------------------------------------

   /**
    *  Return my java class.
    */
   public Class getCls()
   {
      return cls;
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    *  My java class.
    */
   protected Class cls;

   // Private -------------------------------------------------------
}

