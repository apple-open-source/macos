/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.standard;

import org.jboss.test.jbossmx.compliance.standard.support.Torture;

import javax.management.MBeanInfo;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.MalformedObjectNameException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.MBeanRegistrationException;
import javax.management.NotCompliantMBeanException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.ReflectionException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;

import junit.framework.Assert;

public class InfoUtil
{
   static org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(InfoUtil.class);
   
   public static MBeanInfo getMBeanInfo(Object mbean, String name)
   {
      MBeanInfo info = null;

      try
      {
         MBeanServer server = MBeanServerFactory.newMBeanServer();

         ObjectName objectName = new ObjectName(name);
         ObjectInstance instance = server.registerMBean(mbean, objectName);
         info = server.getMBeanInfo(objectName);
      }
      catch (MalformedObjectNameException e)
      {
         Assert.fail("got spurious MalformedObjectNameException");
      }
      catch (InstanceAlreadyExistsException e)
      {
         Assert.fail("got spurious InstanceAlreadyExistsException");
      }
      catch (MBeanRegistrationException e)
      {
         Assert.fail("got spurious MBeanRegistrationException");
      }
      catch (NotCompliantMBeanException e)
      {
         Assert.fail("got spurious NotCompliantMBeanException");
      }
      catch (InstanceNotFoundException e)
      {
         Assert.fail("got spurious InstanceNotFoundException");
      }
      catch (IntrospectionException e)
      {
         Assert.fail("got spurious IntrospectionException");
      }
      catch (ReflectionException e)
      {
         Assert.fail("got spurious ReflectionException");
      }

      return info;
   }

   public static MBeanAttributeInfo findAttribute(MBeanAttributeInfo[] attributes, String name)
   {
      for (int i = 0; i < attributes.length; i++)
      {
         if (attributes[i].getName().equals(name))
         {
            return attributes[i];
         }
      }
      return null;
   }

   public static void dumpConstructors(MBeanConstructorInfo[] constructors)
   {
      log.debug("");
      log.debug("Constructors:");
      for (int i = 0; i < constructors.length; i++)
      {
         StringBuffer dump = new StringBuffer();
         MBeanConstructorInfo constructor = constructors[i];
         dump.append("name=").append(constructor.getName());
         dump.append(",signature=").append(makeSignatureString(constructor.getSignature()));

         log.debug(dump);
      }
   }

   public static void dumpAttributes(MBeanAttributeInfo[] attributes)
   {
      log.debug("");
      log.debug("Attributes:");
      for (int i = 0; i < attributes.length; i++)
      {
         StringBuffer dump = new StringBuffer();
         MBeanAttributeInfo attribute = attributes[i];
         dump.append("name=").append(attribute.getName());
         dump.append(",type=").append(attribute.getType());
         dump.append(",readable=").append(attribute.isReadable());
         dump.append(",writable=").append(attribute.isWritable());
         dump.append(",isIS=").append(attribute.isIs());
         log.debug(dump);
      }
   }

   public static void dumpOperations(MBeanOperationInfo[] operations)
   {
      log.debug("");
      log.debug("Operations:");
      for (int i = 0; i < operations.length; i++)
      {
         StringBuffer dump = new StringBuffer();
         MBeanOperationInfo operation = operations[i];
         dump.append("name=").append(operation.getName());
         dump.append(",impact=").append(decodeImpact(operation.getImpact()));
         dump.append(",returnType=").append(operation.getReturnType());
         dump.append(",signature=").append(makeSignatureString(operation.getSignature()));

         log.debug(dump);
      }
   }

   public static String makeSignatureString(MBeanParameterInfo[] info)
   {
      String[] sig = new String[info.length];
      for (int i = 0; i < info.length; i++)
      {
         sig[i] = info[i].getType();
      }
      return makeSignatureString(sig);
   }

   public static String makeSignatureString(String[] sig)
   {
      StringBuffer buf = new StringBuffer("(");
      for (int i = 0; i < sig.length; i++)
      {
         buf.append(sig[i]);
         if (i != sig.length - 1)
         {
            buf.append(",");
         }
      }
      buf.append(")");
      return buf.toString();
   }

   public static String decodeImpact(int impact)
   {
      switch (impact)
      {
         case MBeanOperationInfo.ACTION:
            return "ACTION";
         case MBeanOperationInfo.ACTION_INFO:
            return "ACTION_INFO";
         case MBeanOperationInfo.INFO:
            return "INFO";
         case MBeanOperationInfo.UNKNOWN:
            return "UNKNOWN";
      }
      throw new IllegalArgumentException("unknown impact value:" + impact);
   }
}
