package org.jboss.ejb;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Method;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.RuntimeOperationsException;

import org.jboss.system.ServiceMBeanSupport;

/** The ContainerRelectionMBean implementation.
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.3 $
 *
 * <p><b>Revisions:</b>
 * <p><b>2001030 Marc Fleury:</b>
 * <ul>
 * <li>I wonder if this class is needed now that we are moving to an MBean per EJB. 
 * <li>In the new design the target EJB (interceptors are detached) should do this logic. 
 *  FIXME: integrate this logic in the target MBean per EJB
 * </ul>
 */
public class ContainerRelection extends ServiceMBeanSupport implements ContainerRelectionMBean
{
   /** Lookup the mbean located under the object name ":service=Container,jndiName=<jndiName>"
    and invoke the getHome and getRemote interfaces and dump the methods for each
    in an html pre block.
    */
   public String inspectEJB(String jndiName)
   {
      MBeanServer server = getServer();
      StringBuffer buffer = new StringBuffer();
      try
      {
         buffer.append("<pre>");
         ObjectName containerName = new ObjectName(":service=Container,jndiName="+jndiName);
         Class homeClass = (Class) server.invoke(containerName, "getHome", null, null);
         buffer.append("\nHome class = "+homeClass);
         buffer.append("\nClassLoader: "+homeClass.getClassLoader());
         buffer.append("\nCodeSource: "+homeClass.getProtectionDomain().getCodeSource());
         buffer.append("\n- Methods:");
         Method[] homeMethods = homeClass.getMethods();
         for(int m = 0; m < homeMethods.length; m ++)
            buffer.append("\n--- "+homeMethods[m]);
         Class remoteClass = (Class) server.invoke(containerName, "getRemote", null, null);
         buffer.append("\nRemote class = "+remoteClass);
         buffer.append("\n- Methods:");
         Method[] remoteMethods = remoteClass.getMethods();
         for(int m = 0; m < remoteMethods.length; m ++)
            buffer.append("\n--- "+remoteMethods[m]);
         buffer.append("\n</pre>\n");
      }
      catch(Throwable e)
      {
         if( e instanceof RuntimeOperationsException )
         {
            RuntimeOperationsException roe = (RuntimeOperationsException) e;
            e = roe.getTargetException();
         }
         StringWriter sw = new StringWriter();
         PrintWriter pw = new PrintWriter(sw);
         e.printStackTrace(pw);
         buffer.append(sw.toString());         
         buffer.append("\n</pre>\n");
      }
      return buffer.toString();
   }

   public String getName()
   {
      return "ContainerRelection";
   }
   
}
