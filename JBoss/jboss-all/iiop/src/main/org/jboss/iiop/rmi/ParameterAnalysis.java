/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import org.omg.CORBA.ParameterMode;


/**
 *  Parameter analysis.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.3 $
 */
public class ParameterAnalysis
   extends AbstractAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(ParameterAnalysis.class);

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   ParameterAnalysis(String javaName, Class cls)
      throws RMIIIOPViolationException
   {
      super(javaName);

      this.cls = cls;

      typeIDLName = Util.getTypeIDLName(cls);
      logger.debug("ParameterAnalysis(): cls=["+cls.getName()+
                   "] typeIDLName=["+typeIDLName+"].");
   }

   // Public --------------------------------------------------------

   /**
    *  Return my attribute mode.
    */
   public ParameterMode getMode()
   {
      // 1.3.4.4 says we always map to IDL "in" parameters.
      return ParameterMode.PARAM_IN;
   }
   
   /**
    *  Return my Java type.
    */
   public Class getCls()
   {
      return cls;
   }
   
   /**
    *  Return the IDL type name of my parameter type.
    */
   public String getTypeIDLName()
   {
      logger.debug("ParameterAnalysis.getTypeIDLName(): cls=["+cls.getName()+
                   "] typeIDLName=["+typeIDLName+"].");
      return typeIDLName;
   }
   
   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  Java type of parameter.
    */
   private Class cls;

   /**
    *  IDL type name of parameter type.
    */
   private String typeIDLName;

}
