package org.jboss.test.tomcat.ejb.bean;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.rmi.RemoteException;
import javax.ejb.*;
import javax.management.MBeanServer;
import javax.naming.InitialContext;
import javax.naming.Context;

/** A simple stateless session bean.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public class StatelessSessionBean implements SessionBean
{
   private SessionContext sessionContext;
   
   public void ejbCreate() throws RemoteException, CreateException
   {
   }
   
   public void ejbActivate() throws RemoteException
   {
   }
   
   public void ejbPassivate() throws RemoteException
   {
   }
   
   public void ejbRemove() throws RemoteException
   {
   }
   
   public void setSessionContext(SessionContext context) throws RemoteException
   {
      sessionContext = context;
   }
   
   public String getMessage()
   {
      return "StatelessSessionBean says: Hello World";
   }
   public String getMessageAndTrace()
   {
      StringBuffer buffer = new StringBuffer("StatelessSessionBean says: Hello World\n");
      buffer.append(getClassLoaders());
      return buffer.toString();
   }
   
   public static String getClassLoaders()
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      StringBuffer buffer = new StringBuffer();
      boolean sawUnifiedClassLoader = false;
      do
      {
         buffer.append("+++ ");
         buffer.append(loader);
         buffer.append('\n');
         String loaderName = loader.getClass().getName();
         boolean isUCL = loaderName.startsWith("org.jboss.");
         if( isUCL && sawUnifiedClassLoader == false )
         {
            sawUnifiedClassLoader = true;
            // Only add the UnifiedLoaderRepository.getURLs info once
            URL[] paths = null;
            try
            {
               Method getAllURLs = loader.getClass().getMethod("getAllURLs", new Class[0]);
               paths = (URL[]) getAllURLs.invoke(loader, new Object[0]);
            }
            catch(Exception e)
            {
            }
            int length = paths == null ? 0 : paths.length;
            for(int p = 0; p < length; p ++)
            {
               buffer.append(" - ");
               buffer.append(paths[p]);
               buffer.append('\n');
            }
         }
         else if( loader instanceof URLClassLoader )
         {
            URLClassLoader uloader = (URLClassLoader) loader;
            URL[] paths = uloader.getURLs();
            int length = paths == null ? 0 : paths.length;
            for(int p = 0; p < length; p ++)
            {
               buffer.append(" - ");
               buffer.append(paths[p]);
               buffer.append('\n');
            }
         }
         if( loader != null )
            loader = loader.getParent();
      } while( loader != null );
      // Write the call stack
      buffer.append("+++ Call stack:\n");
      Throwable t = new RuntimeException();
      StringWriter sw = new StringWriter();
      PrintWriter pw = new PrintWriter(sw);
      t.printStackTrace(pw);
      buffer.append(sw.toString());
      return buffer.toString();
   }
 
}
