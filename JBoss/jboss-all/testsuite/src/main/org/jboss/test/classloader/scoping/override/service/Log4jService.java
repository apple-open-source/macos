package org.jboss.test.classloader.scoping.override.service;

import java.lang.reflect.Method;
import java.net.URL;
import javax.servlet.ServletException;
import org.apache.log4j.Category;
import org.apache.log4j.PropertyConfigurator;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class Log4jService
{
   private Category log;

   public void start() throws Exception
   {
      // Validate the log4j env against the 1.1.3 classes
      try
      {
         Class categoryClass = Category.class;
         // Check that the 1.1.3 assert(boolean, String) method exists
         Class[] sig = {boolean.class, String.class};
         Method m = categoryClass.getDeclaredMethod("assert", sig);
         System.out.println("found assert method: "+m);
         // Find the log4j.properties file
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         URL resURL = loader.getResource("log4j.properties");
         System.out.println("found log4j.properties: "+resURL);
         PropertyConfigurator config = new PropertyConfigurator();
         log = Category.getInstance(Log4jService.class);
         config.configure(resURL);
      }
      catch(Throwable t)
      {
         throw new ServletException("Log4jServlet init failed", t);
      }
   }
}
