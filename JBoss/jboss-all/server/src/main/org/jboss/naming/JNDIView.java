/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.naming;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Proxy;
import java.util.Collection;
import java.util.Iterator;
import java.util.Set;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.LinkRef;
import javax.naming.NameClassPair;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;

import org.jboss.ejb.Container;
import org.jboss.ejb.EjbModule;
import org.jboss.system.ServiceMBeanSupport;

/**
 * A simple utlity mbean that allows one to recursively list the default
 * JBoss InitialContext.
 *
 * @jmx:mbean name="jboss:type=JNDIView"
 *            extends="org.jboss.system.ServiceMBean"
 *            
 * @version <tt>$Revision: 1.19.2.7 $</tt>
 * @author  <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
 * @author  Vladimir Blagojevic <vladimir@xisnext.2y.net>
 * @author  <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 */
public class JNDIView
      extends ServiceMBeanSupport
      implements JNDIViewMBean
{
   /**
    * Provided for JMX compliance.
    */
   public JNDIView()
   {
   }

   /**
    * List deployed application java:comp namespaces, the java:
    * namespace as well as the global InitialContext JNDI namespace.
    *
    * @jmx:managed-operation
    * 
    * @param verbose, if true, list the class of each object in addition to its name
    */
   public String list(boolean verbose)
   {
      StringBuffer buffer = new StringBuffer(4096);
      Set ejbModules = null;
      Context context = null;
      ClassLoader currentLoader = Thread.currentThread().getContextClassLoader();

      // Get all deployed applications so that we can list their
      // java: namespaces which are ClassLoader local

      try
      {
         ejbModules = server.queryNames(EjbModule.EJB_MODULE_QUERY_NAME, null);
      }
      catch (Throwable e)
      {
         log.error("getDeployedApplications failed", e);
         buffer.append("Failed to getDeployedApplications\n");
         formatException(buffer, e);
         buffer.insert(0, "<pre>");
         buffer.append("</pre>");
         return buffer.toString();
      }

      // List each application JNDI namespace
      for (Iterator i = ejbModules.iterator(); i.hasNext();)
      {
         ObjectName app = (ObjectName) i.next();
         String module = app.getKeyProperty("module");
         if( module == null )
            module = app.toString();
         buffer.append("<h1>Ejb Module: " + module + "</h1>\n");
         try
         {
            Collection containers = (Collection) server.getAttribute(app, "Containers");
            for (Iterator iter = containers.iterator(); iter.hasNext();)
            {
               Container con = (Container) iter.next();
               /* Set the thread class loader to that of the container as
                  the class loader is used by the java: context object
                  factory to partition the container namespaces.
               */
               Thread.currentThread().setContextClassLoader(con.getClassLoader());
               String bean = con.getBeanMetaData().getEjbName();
               buffer.append("<h2>java:comp namespace of the " + bean + " bean:</h2>\n");

               try
               {
                  context = new InitialContext();
                  context = (Context) context.lookup("java:comp");
               }
               catch (NamingException e)
               {
                  buffer.append("Failed on lookup, " + e.toString(true));
                  formatException(buffer, e);
                  continue;
               }
               buffer.append("<pre>\n");
               list(context, " ", buffer, verbose);
               buffer.append("</pre>\n");
            }
         }
         catch (Throwable e)
         {
            log.error("getConainers failed", e);
            buffer.append("<pre>");
            buffer.append("Failed to get ejbs in module\n");
            formatException(buffer, e);
            buffer.append("</pre>");
         }
      }

      // List the java: namespace
      Thread.currentThread().setContextClassLoader(currentLoader);
      try
      {
         context = new InitialContext();
         context = (Context) context.lookup("java:");
         buffer.append("<h1>java: Namespace</h1>\n");
         buffer.append("<pre>\n");
         list(context, " ", buffer, verbose);
         buffer.append("</pre>\n");
      }
      catch (NamingException e)
      {
         log.error("lookup for java: failed", e);
         buffer.append("Failed to get InitialContext, " + e.toString(true));
         formatException(buffer, e);
      }

      // List the global JNDI namespace
      try
      {
         context = new InitialContext();
         buffer.append("<h1>Global JNDI Namespace</h1>\n");
         buffer.append("<pre>\n");
         list(context, " ", buffer, verbose);
         buffer.append("</pre>\n");
      }
      catch (NamingException e)
      {
         log.error("Failed to get InitialContext", e);
         buffer.append("Failed to get InitialContext, " + e.toString(true));
         formatException(buffer, e);
      }
      return buffer.toString();
   }

   /**
    * List deployed application java:comp namespaces, the java:
    * namespace as well as the global InitialContext JNDI namespace in a
    * XML Format.
    *
    * @jmx:managed-operation
    *
    * @param verbose, if true, list the class of each object in addition to its name
    **/
   public String listXML()
   {
      StringBuffer buffer = new StringBuffer(4096);
      Set ejbModules = null;
      Context context = null;
      ClassLoader currentLoader = Thread.currentThread().getContextClassLoader();

      /* Get all deployed applications so that we can list their
         java: namespaces which are ClassLoader local
      */
      try
      {
         ejbModules = server.queryNames(EjbModule.EJB_MODULE_QUERY_NAME, null);
      }
      catch (Exception e)
      {
         log.error("getDeployedApplications failed", e);
         openJndiTag(buffer);
         appendErrorTag(buffer,
               "Failed to getDeployedApplications " + e.toString());
         closeJndiTag(buffer);
         return buffer.toString();
      }

      openJndiTag(buffer);
      // List each application JNDI namespace
      for (Iterator i = ejbModules.iterator(); i.hasNext();)
      {
         ObjectName app = (ObjectName) i.next();
         openEjbModuleTag(buffer, app.getKeyProperty("url"));

         listModuleContainers(buffer, app);

         closeEjbModuleTag(buffer);
      }

      // List the java: namespace
      Thread.currentThread().setContextClassLoader(currentLoader);
      try
      {
         context = new InitialContext();
         context = (Context) context.lookup("java:");
      }
      catch (NamingException e)
      {
         log.error("Failed to get InitialContext for (java:)", e);
         appendErrorTag(buffer,
               "Failed to get InitialContext for (java:), " +
               e.toString(true));
      }

      if (context != null)
      {
         openContextTag(buffer);
         appendJavaNameTag(buffer);
         try
         {
            listXML(context, buffer);
         }
         catch (Throwable t)
         {
            log.error("Failed to list contents of (java:)", t);
            appendErrorTag(buffer,
                  "Failed to list contents of (java:), " +
                  t.toString());
         }
         closeContextTag(buffer);

      }   //   if ( context != null )

      // List the global JNDI namespace
      try
      {
         context = new InitialContext();
      }
      catch (NamingException e)
      {
         log.error("Failed to get InitialContext", e);
         appendErrorTag(buffer,
               "Failed to get InitialContext, " + e.toString(true));
      }

      if (context != null)
      {
         openContextTag(buffer);
         appendGlobalNameTag(buffer);
         try
         {
            listXML(context, buffer);
         }
         catch (Throwable t)
         {
            log.error("Failed to list global contents ", t);
            appendErrorTag(buffer,
                  "Failed to list global contents, " + t.toString());
         }
         closeContextTag(buffer);

      }   //   if ( context != null )

      closeJndiTag(buffer);
      return buffer.toString();
   }

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
         throws javax.management.MalformedObjectNameException
   {
      return name == null ? OBJECT_NAME : name;
   }

   private void list(Context ctx, String indent, StringBuffer buffer, boolean verbose)
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      try
      {
         NamingEnumeration ne = ctx.list("");
         while (ne.hasMore())
         {
            NameClassPair pair = (NameClassPair) ne.next();
            log.trace("pair: " + pair);

            String name = pair.getName();
            String className = pair.getClassName();
            boolean recursive = false;
            boolean isLinkRef = false;
            boolean isProxy = false;
            Class c = null;
            try
            {
               c = loader.loadClass(className);
               log.trace("type: " + c);

               if (Context.class.isAssignableFrom(c))
                  recursive = true;
               if (LinkRef.class.isAssignableFrom(c))
                  isLinkRef = true;

               isProxy = Proxy.isProxyClass(c);
            }
            catch (ClassNotFoundException cnfe)
            {
               // If this is a $Proxy* class its a proxy
               if (className.startsWith("$Proxy"))
               {
                  isProxy = true;
                  // We have to get the class from the binding
                  try
                  {
                     Object p = ctx.lookup(name);
                     c = p.getClass();
                  }
                  catch (NamingException e)
                  {
                     Throwable t = e.getRootCause();
                     if (t instanceof ClassNotFoundException)
                     {
                        // Get the class name from the exception msg
                        String msg = t.getMessage();
                        if (msg != null)
                        {
                           // Reset the class name to the CNFE class
                           className = msg;
                        }
                     }
                  }
               }
            }

            buffer.append(indent + " +- " + name);

            // Display reference targets
            if (isLinkRef)
            {
               // Get the 
               try
               {
                  log.trace("looking up LinkRef; name=" + name);
                  Object obj = ctx.lookupLink(name);
                  log.trace("Object type: " + obj.getClass());

                  LinkRef link = (LinkRef) obj;
                  buffer.append("[link -> ");
                  buffer.append(link.getLinkName());
                  buffer.append(']');
               }
               catch (Throwable t)
               {
                  log.debug("Invalid LinkRef for: " + name, t);
                  buffer.append("invalid]");
               }
            }

            // Display proxy interfaces
            if (isProxy)
            {
               buffer.append(" (proxy: " + pair.getClassName());
               if (c != null)
               {
                  Class[] ifaces = c.getInterfaces();
                  buffer.append(" implements ");
                  for (int i = 0; i < ifaces.length; i++)
                  {
                     buffer.append(ifaces[i]);
                     buffer.append(',');
                  }
                  buffer.setCharAt(buffer.length() - 1, ')');
               }
               else
               {
                  buffer.append(" implements " + className + ")");
               }
            }
            else if (verbose)
            {
               buffer.append(" (class: " + pair.getClassName() + ")");
            }

            buffer.append('\n');
            if (recursive)
            {
               try
               {
                  Object value = ctx.lookup(name);
                  if (value instanceof Context)
                  {
                     Context subctx = (Context) value;
                     list(subctx, indent + " |  ", buffer, verbose);
                  }
                  else
                  {
                     buffer.append(indent + " |   NonContext: " + value);
                     buffer.append('\n');
                  }
               }
               catch (Throwable t)
               {
                  buffer.append("Failed to lookup: " + name + ", errmsg=" + t.getMessage());
                  buffer.append('\n');
               }
            }
         }
         ne.close();
      }
      catch (NamingException ne)
      {
         buffer.append("error while listing context " + ctx.toString() + ": " + ne.toString(true));
         formatException(buffer, ne);
      }
   }

   private void listXML(Context ctx, StringBuffer buffer)
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      try
      {
         NamingEnumeration ne = ctx.list("");
         while (ne.hasMore())
         {
            NameClassPair pair = (NameClassPair) ne.next();
            boolean recursive = false;
            boolean isLinkRef = false;
            try
            {
               Class c = loader.loadClass(pair.getClassName());
               if (Context.class.isAssignableFrom(c))
                  recursive = true;
               if (LinkRef.class.isAssignableFrom(c))
                  isLinkRef = true;
            }
            catch (ClassNotFoundException cnfe)
            {
            }

            String name = pair.getName();
            if (isLinkRef)
            {
               Object obj = null;
               LinkRef link = null;
               try
               {
                  obj = ctx.lookupLink(name);
                  link = (LinkRef) obj;
               }
               catch (Throwable t)
               {
                  log.error("Invalid LinkRef for: " + name, t);

                  appendLinkRefErrorTag(buffer);
               }

               appendLinkRefTag(buffer, link, pair);
            }
            else
            {
               if (recursive)
               {
                  Object value = null;

                  try
                  {
                     value = ctx.lookup(name);
                  }
                  catch (Throwable t)
                  {
                     appendErrorTag(buffer,
                           "Failed to lookup: " + name +
                           ", errmsg=" + t.getMessage());
                  }

                  if (value instanceof Context)
                  {
                     Context subctx = (Context) value;
                     openContextTag(buffer);
                     appendNCPTag(buffer, pair);

                     try
                     {
                        listXML(subctx, buffer);
                     }
                     catch (Throwable t)
                     {
                        appendErrorTag(buffer,
                              "Failed to list contents of: " + name +
                              ", errmsg=" + t.getMessage());
                     }

                     closeContextTag(buffer);
                  }
                  else
                  {
                     appendNonContextTag(buffer, pair);
                  }
               }
               else
               {
                  appendLeafTag(buffer, pair);
               }
            }
         }
         ne.close();
      }
      catch (NamingException ne)
      {
         appendErrorTag(buffer,
               "error while listing context " +
               ctx.toString() + ": " + ne.toString(true));
      }
   }

   private void listModuleContainers(StringBuffer buffer, ObjectName app)
   {
      Collection containers = null;

      try
      {
         containers = (Collection) server.getAttribute(app, "Containers");
      }
      catch (Throwable t)
      {
         log.error("getContainers failed", t);
         appendPreExceptionTag(buffer, "Failed to get ejbs in module", t);
      }

      for (Iterator iter = containers.iterator(); iter.hasNext();)
      {
         listContainerContext(buffer, (Container) iter.next());
      }

   }   //   listModuleContainers()

   private void listContainerContext(StringBuffer buffer, Container con)
   {
      /* Set the thread class loader to that of the container as
         the class loader is used by the java: context object
         factory to partition the container namespaces.
      */
      Thread.currentThread().setContextClassLoader(con.getClassLoader());
      String bean = con.getBeanMetaData().getEjbName();
      openContextTag(buffer);
      appendBeanTag(buffer, bean);
      Context context = null;
      try
      {
         context = new InitialContext();
         context = (Context) context.lookup("java:comp");
      }
      catch (NamingException e)
      {
         appendErrorTag(buffer,
               "Failed on lookup " + e.toString(true));
         context = null;
      }

      if (context != null)
      {
         try
         {
            listXML(context, buffer);
         }
         catch (Throwable t)
         {
            appendErrorTag(buffer,
                  "Failed on list contents, " + t.toString());
         }
      }   //   if ( context != null )

      closeContextTag(buffer);

   }   //   listContainerContext()

   private void openJndiTag(StringBuffer buffer)
   {
      buffer.append("<jndi>\n");
   }

   private void closeJndiTag(StringBuffer buffer)
   {
      buffer.append("</jndi>\n");
   }

   private void openEjbModuleTag(StringBuffer buffer, String file)
   {
      buffer.append("<ejbmodule>\n");
      buffer.append("<file>" + file + "</file>\n");
   }

   private void closeEjbModuleTag(StringBuffer buffer)
   {
      buffer.append("</ejbmodule>\n");
   }

   private void appendPreExceptionTag(StringBuffer buffer,
                                      String msg,
                                      Throwable t)
   {
      buffer.append("<pre>\n" + msg+"\n");
      formatException(buffer, t);
      buffer.append("</pre>\n");
   }

   private void appendBeanTag(StringBuffer buffer, String bean)
   {
      buffer.append("<name>java:comp</name>\n");
      buffer.append("<attribute name='bean'>" + bean + "</attribute>\n");
   }

   private void appendJavaNameTag(StringBuffer buffer)
   {
      buffer.append("<name>java:</name>\n");
   }

   private void appendGlobalNameTag(StringBuffer buffer)
   {
      buffer.append("<name>Global</name>\n");
   }

   private void appendLinkRefTag(StringBuffer buffer,
                                 LinkRef link,
                                 NameClassPair ncp)
   {
      buffer.append("<link-ref>\n");
      buffer.append("<name>" + ncp.getName() + "</name>\n");
      try
      {
         String lName = link.getLinkName();
         buffer.append("<link>" + lName + "</link>\n");
      }
      catch (NamingException e)
      {
         appendErrorTag(buffer,
               "Failed to getLinkName, " + e.toString(true));
      }
      buffer.append("<attribute name='class'>" + ncp.getClassName() +
            "</attribute>\n");
      buffer.append("</link-ref>\n");
   }

   private void appendLinkRefErrorTag(StringBuffer buffer)
   {
      buffer.append("<link-ref>\n");
      buffer.append("<name>Invalid</name>\n");
      buffer.append("</link-ref>\n");
   }

   private void openContextTag(StringBuffer buffer)
   {
      buffer.append("<context>\n");
   }

   private void closeContextTag(StringBuffer buffer)
   {
      buffer.append("</context>\n");
   }

   private void appendNonContextTag(StringBuffer buffer, NameClassPair ncp)
   {
      buffer.append("<non-context>\n");
      appendNCPTag(buffer, ncp);
      buffer.append("</non-context>\n");
   }

   private void appendLeafTag(StringBuffer buffer, NameClassPair ncp)
   {
      buffer.append("<leaf>\n");
      appendNCPTag(buffer, ncp);
      buffer.append("</leaf>\n");
   }

   private void appendNCPTag(StringBuffer buffer, NameClassPair ncp)
   {
      buffer.append("<name>" + ncp.getName() + "</name>\n");
      buffer.append("<attribute name='class'>" + ncp.getClassName() +
            "</attribute>\n");
   }

   private void appendErrorTag(StringBuffer buffer, String msg)
   {
      buffer.append("<error>\n");
      buffer.append("<message>" + msg + "</message>\n");
      buffer.append("</error>\n");
   }

   private void formatException(StringBuffer buffer, Throwable t)
   {
      StringWriter sw = new StringWriter();
      PrintWriter pw = new PrintWriter(sw);
      buffer.append("<pre>\n");
      t.printStackTrace(pw);
      buffer.append(sw.toString());
      buffer.append("</pre>\n");
   }
}
