/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.util;
 
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.lang.reflect.Method;
import java.security.DigestOutputStream;
import java.security.MessageDigest;
import java.util.HashMap;
import java.util.Map;
import java.util.WeakHashMap;

/**
 * Create a unique hash for  
 * 
 * @author  <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.7 $
 */
public class MethodHashing
{
   // Constants -----------------------------------------------------
   
   // Static --------------------------------------------------------
   static Map hashMap = new WeakHashMap();

   public static Method findMethodByHash(Class clazz, long hash) throws Exception
   {
      Method[] methods = clazz.getDeclaredMethods();
      for (int i = 0; i < methods.length; i++)
      {
         if (methodHash(methods[i]) == hash) return methods[i];
      }
      if (clazz.getSuperclass() != null)
      {
         return findMethodByHash(clazz.getSuperclass(), hash);
      }
      return null;
   }

   public static long methodHash(Method method)
      throws Exception
   {
      Class[] parameterTypes = method.getParameterTypes();
      String methodDesc = method.getName()+"(";
      for(int j = 0; j < parameterTypes.length; j++)
      {
         methodDesc += getTypeString(parameterTypes[j]);
      }
      methodDesc += ")"+getTypeString(method.getReturnType());
      
      long hash = 0;
      ByteArrayOutputStream bytearrayoutputstream = new ByteArrayOutputStream(512);
      MessageDigest messagedigest = MessageDigest.getInstance("SHA");
      DataOutputStream dataoutputstream = new DataOutputStream(new DigestOutputStream(bytearrayoutputstream, messagedigest));
      dataoutputstream.writeUTF(methodDesc);
      dataoutputstream.flush();
      byte abyte0[] = messagedigest.digest();
      for(int j = 0; j < Math.min(8, abyte0.length); j++)
         hash += (long)(abyte0[j] & 0xff) << j * 8;
      return hash;
   }

   /**
   * Calculate method hashes. This algo is taken from RMI.
   *
   * @param   intf  
   * @return     
   */
   public static Map getInterfaceHashes(Class intf)
   {
      // Create method hashes
      Method[] methods = intf.getDeclaredMethods();
      HashMap map = new HashMap();
      for (int i = 0; i < methods.length; i++)
      {
         Method method = methods[i];
         try
         {
            long hash = methodHash(method);
            map.put(method.toString(), new Long(hash));
         }
         catch (Exception e)
         {
         }
      }
      
      return map;
   }
   
   static String getTypeString(Class cl)
   {
      if (cl == Byte.TYPE)
      {
         return "B";
      } else if (cl == Character.TYPE)
      {
         return "C";
      } else if (cl == Double.TYPE)
      {
         return "D";
      } else if (cl == Float.TYPE)
      {
         return "F";
      } else if (cl == Integer.TYPE)
      {
         return "I";
      } else if (cl == Long.TYPE)
      {
         return "J";
      } else if (cl == Short.TYPE)
      {
         return "S";
      } else if (cl == Boolean.TYPE)
      {
         return "Z";
      } else if (cl == Void.TYPE)
      {
         return "V";
      } else if (cl.isArray())
      {
         return "["+getTypeString(cl.getComponentType());
      } else
      {
         return "L"+cl.getName().replace('.','/')+";";
      }
   }
   
   /*
   * The use of hashCode is not enough to differenciate methods
   * we override the hashCode
   *
   * The hashes are cached in a static for efficiency
   * RO: WeakHashMap needed to support undeploy
   */
   public static long calculateHash(Method method)
   {
      Map methodHashes = (Map)hashMap.get(method.getDeclaringClass());
      
      if (methodHashes == null)
      {
         methodHashes = getInterfaceHashes(method.getDeclaringClass());
         
         // Copy and add
         WeakHashMap newHashMap = new WeakHashMap();
         newHashMap.putAll(hashMap);
         newHashMap.put(method.getDeclaringClass(), methodHashes);
         hashMap = newHashMap;
      }
      
      return ((Long)methodHashes.get(method.toString())).longValue();
   }

}
