/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jmx.interceptors;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.FileInputStream;
import java.io.ObjectInputStream;
import java.lang.reflect.Method;

import javax.naming.Name;
import javax.naming.NameNotFoundException;
import javax.management.MBeanException;
import javax.management.ReflectionException;

import org.jboss.mx.interceptor.AbstractInterceptor;
import org.jboss.mx.interceptor.Invocation;
import org.jboss.mx.interceptor.InvocationException;
import org.jboss.logging.Logger;

/** A simple file based persistence interceptor that saves the value of
 * Naming.bind() calls as serialized objects.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public final class JNDIPersistence
   extends AbstractInterceptor
{
   private static Logger log = Logger.getLogger(JNDIPersistence.class);

   private File storeDirectory;

   public File getStoreDirectory()
   {
      return storeDirectory;
   }
   public void setStoreDirectory(File storeDirectory)
   {
      log.info("setStoreDirectory: "+storeDirectory);
      if( storeDirectory.exists() == false )
         storeDirectory.mkdir();
      this.storeDirectory = storeDirectory;
   }

   // Interceptor overrides -----------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      String opName = invocation.getName();
      log.info("invoke, opName="+opName);

      // If this is not the invoke(Invocation) op just pass it along
      if( opName.equals("invoke") == false )
         return getNext().invoke(invocation);

      Object[] args = invocation.getArgs();
      org.jboss.invocation.Invocation invokeInfo =
         (org.jboss.invocation.Invocation) args[0];

      Object[] iargs = invokeInfo.getArguments();
      for(int a = 0; a < args.length; a ++)
         log.info("  args["+a+"]="+iargs[a]);
      Method method = invokeInfo.getMethod();
      String methodName = method.getName();
      log.info("methodName: "+methodName);
      Object value = null;
      if( methodName.equals("bind") )
      {
         log.info("Dispatching bind");
         getNext().invoke(invocation);
         // Bind succeeded, save the value
         log.info("Saving bind data");
         Name name = (Name) iargs[0];
         Object data = iargs[1];
         try
         {
            writeBinding(name, data);
         }
         catch(Throwable e)
         {
            log.error("Failed to write binding", e);
            throw new InvocationException(e, "Failed to write binding");
         }
      }
      else if( methodName.equals("lookup") )
      {
         log.info("Dispatching lookup");
         try
         {
            value = getNext().invoke(invocation);
            log.info("lookup returned: "+value);
         }
         catch(InvocationException e)
         {
            Throwable ex = getException(e);
            log.info("InvocationException: ", ex);
            if( ex instanceof NameNotFoundException )
            {
               log.info("NameNotFoundException in lookup, finding data");
               Name name = (Name) iargs[0];
               try
               {
                  value = readBinding(name);
                  if( value == null )
                     throw e;
               }
               catch(Throwable e2)
               {
                  log.error("Failed to read binding", e2);
                  throw new InvocationException(e2, "Failed to read binding");
               }
            }
         }
      }
      else
      {
         value = getNext().invoke(invocation);
      }

      return value;
   }

   private void writeBinding(Name name, Object data)
      throws IOException
   {
      File dataFile = new File(storeDirectory, name.toString());
      FileOutputStream fos = new FileOutputStream(dataFile);
      ObjectOutputStream oos = new ObjectOutputStream(fos);
      oos.writeObject(data);
      oos.close();
      fos.close();      
      log.info("Wrote data binding to: "+dataFile);
   }

   private Object readBinding(Name name)
      throws IOException, ClassNotFoundException
   {
      File dataFile = new File(storeDirectory, name.toString());
      if( dataFile.exists() == false )
         return null;

      FileInputStream fis = new FileInputStream(dataFile);
      ObjectInputStream ois = new ObjectInputStream(fis);
      Object data = ois.readObject();
      ois.close();
      fis.close();      
      log.info("Read data binding from: "+dataFile);
      return data;
   }

   /** Unwrap the InvocationException to see what the Naming service
    * exception really was.
    *
    * @param e the wrapped InvocationException
    * @return the underlying initial exception
    */
   Throwable getException(InvocationException e)
   {
      Throwable ex = e.getTargetException();
      if( ex instanceof MBeanException )
      {
         MBeanException mbe = (MBeanException) ex;
         ex = mbe.getTargetException();
      }
      else if( ex instanceof ReflectionException )
      {
         ReflectionException re = (ReflectionException) ex;
         ex = re.getTargetException();
      }
      return ex;
   }
}
