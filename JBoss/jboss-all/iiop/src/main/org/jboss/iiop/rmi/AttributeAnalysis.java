/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import org.omg.CORBA.AttributeMode;

import java.lang.reflect.Method;

import java.rmi.Remote;

/**
 *  Attribute analysis.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.4 $
 */
public class AttributeAnalysis
   extends AbstractAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    *  Create an attribute analysis.
    */
   private AttributeAnalysis(String javaName, AttributeMode mode,
                             Method accessor, Method mutator)
      throws RMIIIOPViolationException
   {
      super(Util.javaToIDLName(javaName), javaName);

      this.mode = mode;
      this.cls = accessor.getReturnType();
      this.accessor = accessor;
      this.mutator = mutator;

      // Only do operation analysis if the attribute is in a remote interface.
      if (accessor.getDeclaringClass().isInterface() &&
          Remote.class.isAssignableFrom(accessor.getDeclaringClass())) {
         accessorAnalysis = new OperationAnalysis(accessor);
         if (mutator != null)
            mutatorAnalysis = new OperationAnalysis(mutator);

         setIDLName(getIDLName()); // Fixup operation names
      }
   }


   /**
    *  Create an attribute analysis for a read-only attribute.
    */
   AttributeAnalysis(String javaName, Method accessor)
      throws RMIIIOPViolationException
   {
      this(javaName, AttributeMode.ATTR_READONLY, accessor, null);
   }

   /**
    *  Create an attribute analysis for a read-write attribute.
    */
   AttributeAnalysis(String javaName, Method accessor, Method mutator)
      throws RMIIIOPViolationException
   {
      this(javaName, AttributeMode.ATTR_NORMAL, accessor, mutator);
   }

   // Public --------------------------------------------------------

   /**
    *  Return my attribute mode.
    */
   public AttributeMode getMode()
   {
      return mode;
   }
   
   /**
    *  Return my Java type.
    */
   public Class getCls()
   {
      return cls;
   }
   
   /**
    *  Return my accessor method
    */
   public Method getAccessor()
   {
      return accessor;
   }
   
   /**
    *  Return my mutator method
    */
   public Method getMutator()
   {
      return mutator;
   }
   
   /**
    *  Return my accessor operation analysis
    */
   public OperationAnalysis getAccessorAnalysis()
   {
      return accessorAnalysis;
   }
   
   /**
    *  Return my mutator operation analysis
    */
   public OperationAnalysis getMutatorAnalysis()
   {
      return mutatorAnalysis;
   }
   
   // Protected -----------------------------------------------------

   // Package protected ---------------------------------------------
 
   /**
    *  Set my unqualified IDL name.
    *  This also sets the names of the associated operations.
    */
   void setIDLName(String idlName)
   {
      super.setIDLName(idlName);

      // If the first char is an uppercase letter and the second char is not
      // an uppercase letter, then convert the first char to lowercase.
      if (idlName.charAt(0) >= 0x41 && idlName.charAt(0) <= 0x5a 
          && (idlName.length() <= 1
              || idlName.charAt(1) < 0x41 || idlName.charAt(1) > 0x5a)) {
         idlName = 
            idlName.substring(0, 1).toLowerCase() + idlName.substring(1);
      }

      if (accessorAnalysis != null)
         accessorAnalysis.setIDLName("_get_" + idlName);
      if (mutatorAnalysis != null)
         mutatorAnalysis.setIDLName("_set_" + idlName);
   }

   // Private -------------------------------------------------------

   /**
    *  Attribute mode.
    */
   private AttributeMode mode;

   /**
    *  Java type.
    */
   private Class cls;

   /**
    *  Accessor Method.
    */
   private Method accessor = null;

   /**
    *  Mutator Method.
    *  This is null for read-only attributes.
    */
   private Method mutator = null;

   /**
    *  Accessor method analysis.
    */
   private OperationAnalysis accessorAnalysis = null;

   /**
    *  Mutator method analysis.
    *  This is null for read-only attributes.
    */
   private OperationAnalysis mutatorAnalysis = null;

}

