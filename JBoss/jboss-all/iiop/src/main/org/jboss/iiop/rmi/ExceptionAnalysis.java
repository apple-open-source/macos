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
 *  Exception analysis.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.4 $
 */
public class ExceptionAnalysis
   extends ValueAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------
 
   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(ExceptionAnalysis.class);

   private static WorkCacheManager cache
                               = new WorkCacheManager(ExceptionAnalysis.class);
 
   public static ExceptionAnalysis getExceptionAnalysis(Class cls)
      throws RMIIIOPViolationException
   {
      return (ExceptionAnalysis)cache.getAnalysis(cls);
   }

   // Constructors --------------------------------------------------

   protected ExceptionAnalysis(Class cls)
   {
      super(cls);
      logger.debug("ExceptionAnalysis(\""+cls.getName()+"\") entered.");
   }
 
   protected void doAnalyze()
      throws RMIIIOPViolationException
   {
      super.doAnalyze();

      if (!Exception.class.isAssignableFrom(cls) ||
          RuntimeException.class.isAssignableFrom(cls))
         throw new RMIIIOPViolationException(
                             "Exception type " + cls.getName() +
                             " must be a checked exception class.", "1.2.6"); 

      // calculate exceptionRepositoryId
      StringBuffer b = new StringBuffer("IDL:");
      b.append(cls.getPackage().getName().replace('.', '/'));
      b.append('/');

      String base = cls.getName();
      base = base.substring(base.lastIndexOf('.')+1);
      if (base.endsWith("Exception"))
         base = base.substring(0, base.length()-9);
      base = Util.javaToIDLName(base + "Ex");

      b.append(base).append(":1.0");
      exceptionRepositoryId = b.toString();
   }

   // Public --------------------------------------------------------

   /**
    *  Return the repository ID for the mapping of this analysis
    *  to an exception.
    */
   public String getExceptionRepositoryId()
   {
      return exceptionRepositoryId;
   }

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   private String exceptionRepositoryId;

   // Inner classes  ------------------------------------------------

}

