/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import org.omg.CORBA.ORB;
import org.omg.CORBA.TCKind;
import org.omg.CORBA.TypeCode;

import java.rmi.Remote;

import java.io.Serializable;
import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectStreamField;

import java.util.ArrayList;
import java.util.Collections;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.Comparator;
import java.util.Map;
import java.util.WeakHashMap;

import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/**
 *  Value analysis.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.4.4.1 $
 */
public class ValueAnalysis
   extends ContainerAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------
 
   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(ValueAnalysis.class);

   private static WorkCacheManager cache
                               = new WorkCacheManager(ValueAnalysis.class);
 
   public static ValueAnalysis getValueAnalysis(Class cls)
      throws RMIIIOPViolationException
   {
      return (ValueAnalysis)cache.getAnalysis(cls);
   }
 
   // Constructors --------------------------------------------------

   protected ValueAnalysis(Class cls)
   {
      super(cls);
      logger.debug("ValueAnalysis(\""+cls.getName()+"\") entered.");
   }
 
   protected void doAnalyze()
      throws RMIIIOPViolationException
   {
      super.doAnalyze();

      if (cls == String.class)
         throw new IllegalArgumentException(
                             "Cannot analyze java.lang.String here: It is a " +
                             "special case."); // 1.3.5.11
                       
      if (cls == Class.class)
         throw new IllegalArgumentException(
                             "Cannot analyze java.lang.Class here: It is a " +
                             "special case."); // 1.3.5.10
                       
      if (Remote.class.isAssignableFrom(cls))
         throw new RMIIIOPViolationException(
                             "Value type " + cls.getName() +
                             " cannot implement java.rmi.Remote.", "1.2.4");

      if (cls.getName().indexOf('$') != -1)
         throw new RMIIIOPNotImplementedException(
                             "Class " + cls.getName() + " has a '$', like " +
                             "proxies or inner classes.");

      externalizable = Externalizable.class.isAssignableFrom(cls);

      if (!externalizable) {
         // Look for serialPersistentFields field.
         Field spf = null;
         try {
            spf = cls.getField("serialPersistentFields");
         } catch (NoSuchFieldException ex) {
            // ignore
         }
         if (spf != null) { // Right modifiers?
            int mods = spf.getModifiers();
            if (!Modifier.isFinal(mods) || !Modifier.isStatic(mods) ||
                !Modifier.isPrivate(mods))
              spf = null; // wrong modifiers
         }
         if (spf != null) { // Right type?
            Class type = spf.getType();
            if (type.isArray()) {
               type = type.getComponentType();
               if (type != ObjectStreamField.class)
                 spf = null; // Array of wrong type
            } else
               spf = null; // Wrong type: Not an array
         }
         if (spf != null) {
            // We have the serialPersistentFields field

            // Get this constant
            try {
               serialPersistentFields = (ObjectStreamField[])spf.get(null);
            } catch (IllegalAccessException ex) {
               throw new RuntimeException("Unexpected IllegalException: " +
                                          ex.toString());
            }

            // Mark this in the fields array
            for (int i = 0; i < fields.length; ++i) {
               if (fields[i] == spf) {
                  f_flags[i] |= F_SPFFIELD;
                  break;
               }
            }
         }

         // Look for a writeObject Method
         Method wo = null;
         try {
            wo = cls.getMethod("writeObject",
                               new Class[] {java.io.OutputStream[].class} );
         } catch (NoSuchMethodException ex) {
            // ignore
         }
         if (wo != null) { // Right return type?
            if (wo.getReturnType() != Void.TYPE)
               wo = null; // Wrong return type
         }
         if (wo != null) { // Right modifiers?
            int mods = spf.getModifiers();
            if (!Modifier.isPrivate(mods))
              wo = null; // wrong modifiers
         }
         if (wo != null) { // Right arguments?
            Class[] paramTypes = wo.getParameterTypes();
            if (paramTypes.length != 1)
               wo = null; // Bad number of parameters
            else if (paramTypes[0] != java.io.OutputStream.class)
               wo = null; // Bad parameter type
         }
         if (wo != null) {
            // We have the writeObject() method.
            hasWriteObjectMethod = true;

            // Mark this in the methods array
            for (int i = 0; i < methods.length; ++i) {
               if (methods[i] == wo) {
                  m_flags[i] |= M_WRITEOBJECT;
                  break;
               }
            }
         }
      }

      // Map all fields not flagged constant or serialPersistentField.
      SortedSet m = new TreeSet(new ValueMemberComparator());

      logger.debug("ValueAnalysis(\""+cls.getName()+"\"): " +
                   "fields.length="+fields.length);
      for (int i = 0; i < fields.length; ++i) {
         logger.debug("ValueAnalysis(\""+cls.getName()+"\"): " +
                      "Considering field["+i+"] \"" + fields[i].getName() + 
                      "\"" + " f_flags=" + f_flags[i]);
         if (f_flags[i] != 0)
            continue; // flagged

         int mods = fields[i].getModifiers();
         logger.debug("ValueAnalysis(\""+cls.getName()+"\"): mods=" + mods);
         if (Modifier.isStatic(mods) || Modifier.isTransient(mods))
            continue; // don't map this

         ValueMemberAnalysis vma;
         vma = new ValueMemberAnalysis(fields[i].getName(),
                                       fields[i].getType(),
                                       Modifier.isPublic(mods));
         m.add(vma);
      }

      members = new ValueMemberAnalysis[m.size()];
      members = (ValueMemberAnalysis[])m.toArray(members);
      logger.debug("ValueAnalysis(\""+cls.getName()+"\") value member count: "
                   + members.length);
      
      // Get superclass analysis
      Class superClass = cls.getSuperclass();
      if (superClass == java.lang.Object.class)
         superClass = null;
      if (superClass == null)
         superAnalysis = null;
      else {
         logger.debug("ValueAnalysis(\""+cls.getName()+"\"): superclass: " +
                      superClass.getName());
         superAnalysis = getValueAnalysis(superClass);
      }

      if (!Serializable.class.isAssignableFrom(cls))
         abstractValue = true;

      fixupCaseNames();

      logger.debug("ValueAnalysis(\""+cls.getName()+"\") done.");
   }

   // Public --------------------------------------------------------

   /**
    *  Returns the superclass analysis, or null if this inherits from
    *  java.lang.Object.
    */
   public ValueAnalysis getSuperAnalysis()
   {
      return superAnalysis;
   }

   /**
    *  Returns true if this value is abstract.
    */
   public boolean isAbstractValue()
   {
      return abstractValue;
   }

   /**
    *  Returns true if this value is custom.
    */
   public boolean isCustom()
   {
      return externalizable || hasWriteObjectMethod;
   }

   /**
    *  Returns true if this value implements java.io.Externalizable.
    */
   public boolean isExternalizable()
   {
      return externalizable;
   }

   /**
    *  Return the value members of this value class.
    */
   public ValueMemberAnalysis[] getMembers()
   {
      return (ValueMemberAnalysis[])members.clone();
   }


   // Protected -----------------------------------------------------

   /**
    *  Analyse attributes.
    *  This will fill in the <code>attributes</code> array.
    *  Here we override the implementation in ContainerAnalysis and create an 
    *  empty array, because for valuetypes we don't want to analyse IDL 
    *  attributes or operations (as in "rmic -idl -noValueMethods").
    */
   protected void analyzeAttributes()
      throws RMIIIOPViolationException
   {
      attributes = new AttributeAnalysis[0];
   }

   /**
    *  Return a list of all the entries contained here.
    *
    *  @param entries The list of entries contained here. Entries in this list
    *                 are subclasses of <code>AbstractAnalysis</code>.
    */
   protected ArrayList getContainedEntries()
   {
      ArrayList ret = new ArrayList(constants.length +
                                    attributes.length +
                                    members.length);
 
      for (int i = 0; i < constants.length; ++i)
         ret.add(constants[i]);
      for (int i = 0; i < attributes.length; ++i)
         ret.add(attributes[i]);
      for (int i = 0; i < members.length; ++i)
         ret.add(members[i]);

      return ret;
   }


   // Private -------------------------------------------------------

   /**
    *  Analysis of our superclass, of null if our superclass is
    *  java.lang.Object.
    */
   ValueAnalysis superAnalysis;

   /**
    *  Flags that this is an abstract value.
    */
   private boolean abstractValue = false;

   /**
    *  Flags that this implements <code>java.io.Externalizable</code>.
    */
   private boolean externalizable = false;

   /**
    *  Flags that this has a <code>writeObject()</code> method.
    */
   private boolean hasWriteObjectMethod = false;

   /**
    *  The <code>serialPersistentFields of the value, or <code>null</code>
    *  if the value does not have this field.
    */
   private ObjectStreamField[] serialPersistentFields;

   /**
    *  The value members of this value class.
    */
   private ValueMemberAnalysis[] members;


   // Inner classes  ------------------------------------------------

   /**
    *  A <code>Comparator</code> for the field ordering specified at the
    *  end of section 1.3.5.6.
    */
   private static class ValueMemberComparator
      implements Comparator
   {
      public int compare(Object o1, Object o2)
      {
         if (o1 == o2)
            return 0;

         ValueMemberAnalysis m1 = (ValueMemberAnalysis)o1;
         ValueMemberAnalysis m2 = (ValueMemberAnalysis)o2;

         boolean p1 = m1.getCls().isPrimitive();
         boolean p2 = m2.getCls().isPrimitive();

         if (p1 && !p2)
            return -1;
         if (!p1 && p2)
            return 1;

         return m1.getJavaName().compareTo(m2.getJavaName());
      }
   }

}
