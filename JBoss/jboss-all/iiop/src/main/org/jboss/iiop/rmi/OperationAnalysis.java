/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.rmi;

import java.rmi.Remote;
import java.rmi.RemoteException;

import java.lang.reflect.Method;

import java.util.ArrayList;


/**
 *  Operation analysis.
 *
 *  Routines here are conforming to the "Java(TM) Language to IDL Mapping
 *  Specification", version 1.1 (01-06-07).
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.4.4.1 $
 */
public class OperationAnalysis
   extends AbstractAnalysis
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(OperationAnalysis.class);

   // Constructors --------------------------------------------------

   OperationAnalysis(Method method)
      throws RMIIIOPViolationException
   {
      super(method.getName());
      logger.debug("new OperationAnalysis: " + method.getName());
      this.method = method;

      // Check if valid return type, IF it is a remote interface.
      Class retCls = method.getReturnType();
      if (retCls.isInterface() && Remote.class.isAssignableFrom(retCls))
         Util.isValidRMIIIOP(retCls);

      // Analyze exceptions
      Class[] ex = method.getExceptionTypes();
      boolean gotRemoteException = false;
      ArrayList a = new ArrayList();
      for (int i = 0; i < ex.length; ++i) {
         if (ex[i].isAssignableFrom(java.rmi.RemoteException.class))
            gotRemoteException = true;
         if (Exception.class.isAssignableFrom(ex[i]) &&
             !RuntimeException.class.isAssignableFrom(ex[i]) &&
             !RemoteException.class.isAssignableFrom(ex[i]) )
           a.add(ExceptionAnalysis.getExceptionAnalysis(ex[i])); // map this
      }
      if (!gotRemoteException && 
          Remote.class.isAssignableFrom(method.getDeclaringClass()))
         throw new RMIIIOPViolationException(
              "All interface methods must throw java.rmi.RemoteException, " +
              "or a superclass of java.rmi.RemoteException, but method " +
              getJavaName() + " of interface " +
              method.getDeclaringClass().getName() + " does not.", "1.2.3");
      mappedExceptions = new ExceptionAnalysis[a.size()];
      mappedExceptions = (ExceptionAnalysis[])a.toArray(mappedExceptions);

      // Analyze parameters
      Class[] params = method.getParameterTypes();
      parameters = new ParameterAnalysis[params.length];
      for (int i = 0; i < params.length; ++i) {
         logger.debug("OperationAnalysis: " + method.getName() + 
                      " has parameter [" + params[i].getName() + "]");
         parameters[i] = new ParameterAnalysis("param" + (i+1), params[i]);
      }
   }

   // Public --------------------------------------------------------

   /**
    *  Return my Java return type.
    */
   public Class getReturnType()
   {
      return method.getReturnType();
   }
 
   /**
    *  Return my mapped Method.
    */
   public Method getMethod()
   {
      return method;
   }
   
   /**
    *  Return my mapped exceptions.
    */
   public ExceptionAnalysis[] getMappedExceptions()
   {
      return (ExceptionAnalysis[])mappedExceptions.clone();
   }
   
   /**
    *  Return my parameters.
    */
   public ParameterAnalysis[] getParameters()
   {
      return (ParameterAnalysis[])parameters.clone();
   }
   
   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  The Method that this OperationAnalysis is mapping.
    */
   private Method method;

   /**
    *  The mapped exceptions of this operation.
    */
   private ExceptionAnalysis[] mappedExceptions;

   /**
    *  The parameters of this operation.
    */
   private ParameterAnalysis[] parameters;

}
