/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import org.omg.CORBA.Any;


/**
 *  Abstract base class for all analysis classes.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
abstract class AbstractAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   AbstractAnalysis(String idlName, String javaName)
   {
      this.idlName = idlName;
      this.javaName = javaName;
   }

   AbstractAnalysis(String javaName)
   {
      this(Util.javaToIDLName(javaName), javaName);
   }

   // Public --------------------------------------------------------

   /**
    *  Return my unqualified IDL name.
    */
   public String getIDLName()
   {
      return idlName;
   }

   /**
    *  Return my unqualified java name.
    */
   public String getJavaName()
   {
      return javaName;
   }

   // Package protected ---------------------------------------------

   /**
    *  Set my unqualified IDL name.
    */
   void setIDLName(String idlName)
   {
      this.idlName = idlName;
   }

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  My unqualified IDL name.
    */
   private String idlName;

   /**
    *  My unqualified java name.
    */
   private String javaName;

}

