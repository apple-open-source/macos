/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.system;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.DynamicMBean;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.ReflectionException;

import org.jboss.logging.Logger;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>6 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class ServiceDynamicMBeanSupport 
   extends ServiceMBeanSupport
   implements DynamicMBean
{

  // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public ServiceDynamicMBeanSupport()
   {
      super();
   }

   public ServiceDynamicMBeanSupport(Class type)
   {
      super(type);
   }

   public ServiceDynamicMBeanSupport(String category)
   {
      super(category);
   }

   public ServiceDynamicMBeanSupport(Logger log)
   {
      super(log);
   }
   
   // Public --------------------------------------------------------

   // DynamicMBean implementation -----------------------------------
   
   public Object getAttribute(String attribute)
      throws AttributeNotFoundException, MBeanException, ReflectionException
   {
      // locally managed attributes!
      //
      if("State".equals(attribute))
      {
         return new Integer(getState());
      }
      if("StateString".equals(attribute))
      {
         return getStateString();
      }
      if("Name".equals(attribute))
      {
         return getName();
      }
      
      // Wrapped attributes?
      //
      return getInternalAttribute (attribute);
      
   }

   public Object invoke(String actionName, Object[] params, String[] signature)
      throws MBeanException, ReflectionException
   {
      if (params == null || params.length == 0) 
      {
         try 
         {
            if ("create".equals(actionName)) 
            {
               create(); return null;
            }
            else if ("start".equals(actionName)) 
            {
               start(); return null;
            }
            else if ("stop".equals(actionName)) 
            {
               stop(); return null;
            }
            else if ("destroy".equals(actionName)) 
            {
               destroy(); return null;
            }
            
         }
         catch (Exception e)
         {
            throw new MBeanException(e, 
                  "Exception in service lifecyle operation: " + actionName);
         }         
      }
      
      // If I am here, it means that the invocation has not been handled locally
      //
      try
      {
         return internalInvoke (actionName, params, signature);
      }
      catch (Exception e)
      {
         throw new MBeanException(e, 
               "Exception invoking: " + actionName);
      }         
   }

   public void setAttribute(Attribute attribute)
      throws
         AttributeNotFoundException,
         InvalidAttributeValueException,
         MBeanException,
         ReflectionException
   {
      setInternalAttribute (attribute);
   }

   public AttributeList setAttributes(AttributeList arg0)
   {
      return null;
   }

   public AttributeList getAttributes(String[] arg0)
   {
      return null;
   }

   public MBeanInfo getMBeanInfo()
   {
      MBeanParameterInfo[] noParams = new MBeanParameterInfo[] {};
      
      MBeanConstructorInfo[] ctorInfo = new  MBeanConstructorInfo[] {};
      
      MBeanAttributeInfo[] attrInfo = new MBeanAttributeInfo[] {
       new MBeanAttributeInfo("Name",
               "java.lang.String",
               "Return the service name",
               true,
               false,
               false),
       new MBeanAttributeInfo("State",
               "int",
               "Return the service state",
               true,
               false,
               false),
       new MBeanAttributeInfo("StateString",
               "java.lang.String",
               "Return the service's state as a String",
               true,
               false,
               false)
      };
      
      MBeanOperationInfo[] opInfo = {
         new MBeanOperationInfo("create",
                                "create service lifecycle operation",
                                noParams,
                                "void",
                                MBeanOperationInfo.ACTION),
         
         new MBeanOperationInfo("start",
                                "start service lifecycle operation",
                                noParams,
                                "void",
                                MBeanOperationInfo.ACTION),
         
         new MBeanOperationInfo("stop",
                                "stop service lifecycle operation",
                                noParams,
                                "void",
                                MBeanOperationInfo.ACTION),
         
         new MBeanOperationInfo("destroy",
                                "destroy service lifecycle operation",
                                noParams,
                                "void",
                                MBeanOperationInfo.ACTION),                                
      };
      
      MBeanNotificationInfo[] notifyInfo = null;
      return new MBeanInfo(getClass().getName(), 
                           "Dynamic MBean Service",
                           attrInfo, 
                           ctorInfo, 
                           opInfo, 
                           notifyInfo);
   }

   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected Object getInternalAttribute(String attribute)
      throws AttributeNotFoundException, MBeanException, ReflectionException
      {
         throw new AttributeNotFoundException ("getInternalAttribute not implemented");
      }
   
   protected void setInternalAttribute(Attribute attribute)
      throws
         AttributeNotFoundException,
         InvalidAttributeValueException,
         MBeanException,
         ReflectionException
   {
      throw new AttributeNotFoundException ("setInternalAttribute not implemented");
   }

   protected Object internalInvoke(String actionName, Object[] params, String[] signature)
      throws MBeanException, ReflectionException
   {
      throw new MBeanException (new Exception(), "internalInvoke not implemented");
   }
   
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
