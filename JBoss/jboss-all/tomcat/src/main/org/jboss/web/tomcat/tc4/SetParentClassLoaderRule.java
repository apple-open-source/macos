package org.jboss.web.tomcat.tc4;

import java.io.IOException;
import java.net.URL;
import java.net.JarURLConnection;
import java.util.HashSet;

import org.apache.catalina.Context;
import org.apache.catalina.Loader;
import org.apache.catalina.loader.WebappLoader;
import org.apache.commons.digester.Rule;

/** This action sets the Context parent class loader to the loader passed into
 the ctor and adds the urls for the jars that contain the javax.servlet.Servlet
 class, the org/apache/jasper/JspC.class, and the
 org/apache/jasper/runtime/HttpJspBase.class.

@author Scott.Stark@jboss.org
@version $Revision: 1.1.1.1 $
 */
public class SetParentClassLoaderRule extends Rule
{
   ClassLoader loader;

   public SetParentClassLoaderRule(ClassLoader loader)
   {
      this.loader = loader;
   }

   /** The stack must look like Embedded/Service/Engine/Host/Context
    */
   public void end() throws Exception
   {
      Context context = (Context) digester.peek();
      context.setParentClassLoader(loader);

      // Locate the Servlet and JSP jars
      HashSet cp = new HashSet();
      String path0 = getResource("javax/servlet/Servlet.class", loader);
      if( path0 != null )
         cp.add(path0);
      String path1 = getResource("org/apache/jasper/JspC.class", loader);
      if( path1 != null )
         cp.add(path1);
      String path2 = getResource("org/apache/jasper/runtime/HttpJspBase.class", loader);
      if( path2 != null )
         cp.add(path2);

      // Add the Servlet/JSP jars to the web context classpath
      Loader ctxLoader = context.getLoader();
      if( ctxLoader == null )
      {
         ctxLoader = new WebappLoader(loader);
         context.setLoader(ctxLoader);
      }
      String[] jars = new String[cp.size()];
      cp.toArray(jars);
      for(int j = 0; j < jars.length; j ++)
         ctxLoader.addRepository(jars[j]);
   }

   private String getResource(String name, ClassLoader loader) throws IOException
   {
      URL res = loader.getResource(name);
      if( res == null )
         return null;

      if( res.getProtocol().equals("jar") )
      {
         JarURLConnection jarConn = (JarURLConnection) res.openConnection();
         res = jarConn.getJarFileURL();
      }
      return res.toExternalForm();
   }
}
