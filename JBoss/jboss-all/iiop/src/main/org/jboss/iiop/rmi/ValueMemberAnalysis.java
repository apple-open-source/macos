/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;


/**
 *  Value member analysis.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public class ValueMemberAnalysis
   extends AbstractAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   ValueMemberAnalysis(String javaName, Class cls, boolean publicMember)
   {
      super(javaName);

      this.cls = cls;
      this.publicMember = publicMember;
   }

   // Public --------------------------------------------------------

   /**
    *  Return my Java type.
    */
   public Class getCls()
   {
      return cls;
   }
   
   /**
    *  Returns true iff this member has private visibility.
    */
   public boolean isPublic()
   {
      return publicMember;
   }

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  Java type.
    */
   private Class cls;

   /**
    *  Flags that this member is public.
    */
   private boolean publicMember;

}

