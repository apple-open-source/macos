/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.j2ee;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import javax.management.Attribute;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

import org.jboss.logging.Logger;

import org.w3c.dom.Document;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.JavaMailResource JavaMailResource}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.7.2.4 $
 * @jmx:mbean extends="org.jboss.management.j2ee.StateManageable,org.jboss.management.j2ee.J2EEResourceMBean"
 **/
public class JavaMailResource
      extends J2EEResource
      implements JavaMailResourceMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JavaMailResource.class);

   public static final String J2EE_TYPE = "JavaMailResource";

   // Attributes ----------------------------------------------------

   private StateManagement mState;
   private ObjectName mailServiceName;

   // Static --------------------------------------------------------

   public static ObjectName create(MBeanServer mbeanServer, String resName,
      ObjectName mailServiceName)
   {
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);
      ObjectName jsr77Name = null;
      try
      {
         JavaMailResource mailRes = new JavaMailResource(resName, j2eeServerName, mailServiceName);
         jsr77Name = mailRes.getObjectName();
         mbeanServer.registerMBean(mailRes, jsr77Name);
         log.debug("Created JSR-77 JavaMailResource: " + resName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 JavaMailResource: " + resName, e);
      }
      return jsr77Name;
   }

   public static void destroy(MBeanServer mbeanServer, String resName)
   {
      try
      {
         J2EEManagedObject.removeObject(
               mbeanServer,
               J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + JavaMailResource.J2EE_TYPE + "," +
               "name=" + resName + "," +
               "*"
         );
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 JNDIResource: " + resName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param resName Name of the JavaMailResource
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    **/
   public JavaMailResource(String resName, ObjectName j2eeServerName,
      ObjectName mailServiceName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, resName, j2eeServerName);
      this.mailServiceName = mailServiceName;
      mState = new StateManagement(this);
   }

   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public String getUserName()
         throws Exception
   {
      return (String) server.getAttribute(mailServiceName, "User");
   }

   /**
    * @jmx:managed-attribute
    **/
   public void setUserName(String pName)
         throws Exception
   {
      server.setAttribute(mailServiceName, new Attribute("User", pName));
   }

   /**
    * @jmx:managed-attribute
    **/
   public void setPassword(String pPassword)
         throws Exception
   {
      server.setAttribute(mailServiceName, new Attribute("Password", pPassword));
   }

   /**
    * @jmx:managed-attribute
    **/
   public String getJNDIName()
         throws Exception
   {
      return (String) server.getAttribute(mailServiceName, "JNDIName");
   }

   /**
    * @jmx:managed-attribute
    **/
   public void setJNDIName(String pName)
         throws Exception
   {
      server.setAttribute(mailServiceName, new Attribute("JNDIName", pName));
   }

   /**
    * @jmx:managed-attribute
    **/
   public String getConfiguration()
         throws Exception
   {
      return server.getAttribute(mailServiceName, "Configuration") + "";
   }

   /**
    * @jmx:managed-attribute
    **/
   public void setConfiguration(String pConfigurationElement)
         throws Exception
   {
      if (pConfigurationElement == null || pConfigurationElement.length() == 0)
      {
         pConfigurationElement = "<configuration/>";
      }
      DocumentBuilder lParser = DocumentBuilderFactory.newInstance().newDocumentBuilder();
      InputStream lInput = new ByteArrayInputStream(pConfigurationElement.getBytes());
      Document lDocument = lParser.parse(lInput);
      server.setAttribute(mailServiceName, new Attribute("Configuration", lDocument.getDocumentElement()));
   }

   // javax.managment.j2ee.EventProvider implementation -------------

   public String[] getEventTypes()
   {
      return StateManagement.stateTypes;
   }

   public String getEventType(int pIndex)
   {
      if (pIndex >= 0 && pIndex < StateManagement.stateTypes.length)
      {
         return StateManagement.stateTypes[pIndex];
      }
      else
      {
         return null;
      }
   }

   // javax.management.j2ee.StateManageable implementation ----------

   public long getStartTime()
   {
      return mState.getStartTime();
   }

   public int getState()
   {
      return mState.getState();
   }

   public void mejbStart()
   {
      try
      {
         server.invoke(
               mailServiceName,
               "start",
               new Object[]{},
               new String[]{}
         );
      }
      catch (Exception e)
      {
         log.error("start failed", e);
      }
   }

   public void mejbStartRecursive()
   {
      // No recursive start here
      try
      {
         mejbStart();
      }
      catch (Exception e)
      {
         log.error("start failed", e);
      }
   }

   public void mejbStop()
   {
      try
      {
         server.invoke(
               mailServiceName,
               "stop",
               new Object[]{},
               new String[]{}
         );
      }
      catch (Exception e)
      {
         log.error("Stop of JavaMailResource failed", e);
      }
   }

   public void postCreation()
   {
      try
      {
         server.addNotificationListener(mailServiceName, mState, null, null);
      }
      catch (JMException e)
      {
         log.debug("Failed to add notification listener", e);
      }
      sendNotification(StateManagement.CREATED_EVENT, "Java Mail Resource created");
   }

   public void preDestruction()
   {
      sendNotification(StateManagement.DESTROYED_EVENT, "Java Mail Resource deleted");
      // Remove the listener of the target MBean
      try
      {
         server.removeNotificationListener(mailServiceName, mState);
      }
      catch (JMException jme)
      {
         // When the service is not available anymore then just ignore the exception
      }
   }

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JavaMailResource { " + super.toString() + " } [ " +
            " ]";
   }
}
