package org.jboss.web.tomcat.mbean;

import java.net.URL;
import java.util.Properties;
import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanRegistrationException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.NotCompliantMBeanException;
import javax.management.AttributeNotFoundException;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import org.apache.catalina.core.StandardWrapper;
import org.jboss.mx.modelmbean.XMBean;

/** Initial version of the servlet mbean
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.1.1 $
 */
public class ServletInfo
{
   private StandardWrapper servlet;

   public static ObjectName createServletMBean(StandardWrapper servlet, String context,
      String vhost, ObjectName webContainer, MBeanServer server)
      throws MalformedObjectNameException, MBeanRegistrationException,
      InstanceAlreadyExistsException, MBeanException, NotCompliantMBeanException
   {
      Properties props = new Properties();
      props.setProperty("name", servlet.getName());
      
      if (context==null || context.equals(""))
      {
         context="/";
      }

      props.setProperty("context", context);
      props.setProperty("vhost", vhost == null ? "" : vhost);
      ObjectName oname = new ObjectName(webContainer.getDomain(), props);
      ServletInfo info = new ServletInfo(servlet);
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      URL servletInfoURL = loader.getResource("META-INF/servletinfo-xmbean.xml");
      XMBean xmbean = new XMBean(info, servletInfoURL);
      server.registerMBean(xmbean, oname);
      return oname;
   }

   public ServletInfo(StandardWrapper servlet)
   {
      this.servlet = servlet;
   }

   protected Object getInternalAttribute(String attribute)
   throws AttributeNotFoundException, MBeanException, ReflectionException
   {
      throw new AttributeNotFoundException ("getInternalAttribute not implemented");
   }

   public String getName()
   {
      return servlet.getName();
   }

   public long getProcessingTime()
   {
       return servlet.getProcessingTime();
   }

   /** This is not supported by the tomcat StandardWrapper */
   public long getMinTime()
   {
      return 0;
   }
   public long getMaxTime()
   {
       return servlet.getMaxTime();
   }

   public int getRequestCount()
   {
       return servlet.getRequestCount();
   }

   public int getErrorCount()
   {
       return servlet.getErrorCount();
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer("ServletInfo[");
      tmp.append("name: ");
      tmp.append(getName());
      tmp.append(",time: ");
      tmp.append(getProcessingTime());
      tmp.append(",maxTime: ");
      tmp.append(getMaxTime());
      tmp.append(",requestCount: ");
      tmp.append(getRequestCount());
      tmp.append(",errorCount: ");
      tmp.append(getErrorCount());
      tmp.append("]");
      return tmp.toString();
   }
}
