/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.singleton;

import java.security.InvalidParameterException;

import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MBeanException;
import javax.management.ObjectName;
import javax.management.ReflectionException;

/**
 * 
 * Allows singleton MBeans to be
 * added declaratively at deployment time
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 * @author Scott.Stark@jboss.org
 * @author <a href="mailto:mr@gedoplan.de">Marcus Redeker</a>
 * @version $Revision: 1.1.2.4 $
 */
public class HASingletonController
   extends HASingletonSupport
   implements HASingletonControllerMBean
{

   // -------------------------------------------------------------------------
   // Constants
   // -------------------------------------------------------------------------

   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   private ObjectName mSingletonMBean;
   private String mSingletonMBeanStartMethod;
   private String mSingletonMBeanStartMethodArgument;
   private String mSingletonMBeanStopMethod;
   private String mSingletonMBeanStopMethodArgument;

   private static final Object[] NO_ARGS = new Object[0];
   private static final String[] NO_TYPES = new String[0];

   /*
    * 
    * Forwards the call to the target start method
    * 
    * @see org.jboss.ha.singleton.HASingletonSupport#startSingleton()
    */
   public void startSingleton()
   {
      super.startSingleton();

      try
      {
         log.debug("Starting: "+mSingletonMBean+" using: "+mSingletonMBeanStartMethod);
         invokeSingletonMBeanMethod(
            mSingletonMBean,
            mSingletonMBeanStartMethod,
            mSingletonMBeanStartMethodArgument 
            );
      }
      catch (JMException jme)
      {
         log.error("Controlled Singleton MBean failed to become master", jme);
      }
   }


   /*
    * 
    * Forwards the call to the target stop method
    * 
    * @see org.jboss.ha.singleton.HASingletonSupport#stopSingleton()
    */
   public void stopSingleton()
   {
      super.stopSingleton();

      try
      {
         log.debug("Stopping: "+mSingletonMBean+" using: "+mSingletonMBeanStopMethod);
         invokeSingletonMBeanMethod(
            mSingletonMBean,
            mSingletonMBeanStopMethod,
            mSingletonMBeanStopMethodArgument
            );
      }
      catch (JMException jme)
      {
         log.error("Controlled Singleton MBean failed to resign from master position", jme);
      }
   }

   protected Object invokeSingletonMBeanMethod(ObjectName name,
      String operationName, Object param)
      throws InstanceNotFoundException, MBeanException, ReflectionException
   {
     Object[] params = NO_ARGS;
     String[] signature = NO_TYPES;
         
     if (param != null) 
     {
       params = new Object[] {param};
       signature = new String[] {param.getClass().getName()};
     }
     
     return server.invoke(name, operationName, params, signature);
   }

   public ObjectName getTargetName()
   {
      return mSingletonMBean;
   }

   public void setTargetName(ObjectName targetObjectName)
   {
      this.mSingletonMBean = targetObjectName;
   }

   public String getTargetStartMethod()
   {
      return mSingletonMBeanStartMethod;
   }

   public void setTargetStartMethod(String targetStartMethod)
      throws InvalidParameterException
   {
      String methodName = null;
      if (targetStartMethod == null)
      {
         methodName = "";
      }
      else
      {
         methodName = targetStartMethod.trim();
      }
      if (methodName.equals(""))
      {
         methodName = "startSingleton";
      }
      mSingletonMBeanStartMethod = methodName;
   }


   public String getTargetStopMethod()
   {
      return mSingletonMBeanStopMethod;
   }

   public void setTargetStopMethod(String targetStopMethod)
      throws InvalidParameterException
   {
      String methodName = null;
      if (targetStopMethod == null)
      {
         methodName = "";
      }
      else
      {
         methodName = targetStopMethod.trim();
      }
      if (methodName.equals(""))
      {
         methodName = "stopSingleton";
      }
      mSingletonMBeanStopMethod = methodName;
   }


  public String getTargetStartMethodArgument()
  {
    return mSingletonMBeanStartMethodArgument ;
  }

  public void setTargetStartMethodArgument(String targetStartMethodArgument)
  {
    mSingletonMBeanStartMethodArgument = targetStartMethodArgument;
  }

  public String getTargetStopMethodArgument()
  {
    return mSingletonMBeanStopMethodArgument ;
  }

  public void setTargetStopMethodArgument(String targetStopMethodArgument)
  {
    mSingletonMBeanStopMethodArgument =  targetStopMethodArgument;
    
  }

}
