/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.adaptor.xml;

import java.io.ByteArrayInputStream;
import java.util.Iterator;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import javax.naming.InitialContext;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

import org.jboss.system.ServiceMBeanSupport;

import org.w3c.dom.Document;

/**
 * MBean Wrapper for the XML Test-Adaptor.
 *
 * @jmx:mbean name="jboss.jmx:type=XMLTestService"
 *            extends="org.jboss.system.ServiceMBean"
 *            
 * @author Andreas Schaefer (andreas.schaefer@madplanet.com)
 * @created June 22, 2001
 * @version $Revision: 1.5 $
 */
public class XMLTestService
   extends ServiceMBeanSupport
   implements XMLTestServiceMBean
{
   private ObjectName lXMLAdaptor;
  
   public static String JNDI_NAME = "jmx:test:xml";

   protected ObjectName getObjectName( MBeanServer pServer, ObjectName pName )
      throws MalformedObjectNameException
   {
      return pName == null ? OBJECT_NAME : pName;
   }

   protected void startService()
      throws Exception
   {
      // Get XML-Adaptor
      log.debug( "Lookup the XML Adaptor" );
      
      Iterator i = server.queryNames( new ObjectName( "jmx:name=xml" ), null ).iterator();
      
      if( i.hasNext() ) {
         ObjectName lName = (ObjectName) i.next();
         log.debug( "Got object name: " + lName );
         
         // Create Test XML Document
         Document lTest = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(
            new ByteArrayInputStream(new String(
                                        "<jmx>" +
                                        "<invoke operation=\"stop\"><object-name name=\":service=Scheduler\"/></invoke>" +
                                        "<create-mbean code=\"org.jboss.util.Scheduler\">" +
                                        "<object-name name=\":service=Scheduler\"/>" +
                                        "<constructor>" +
                                        "<argument type=\"java.lang.String\">:server=Scheduler</argument>" +
                                        "<argument type=\"java.lang.String\">org.jboss.util.Scheduler$SchedulableExample</argument>" +
                                        "<argument type=\"java.lang.String\">Schedulable Test,12345</argument>" +
                                        "<argument type=\"java.lang.String\">java.lang.String,int</argument>" +
                                        "<argument type=\"long\">0</argument>" +
                                        "<argument type=\"long\">10000</argument>" +
                                        "<argument type=\"long\">-1</argument>" +
                                        "</constructor>" +
                                        "</create-mbean>" +
                                        "<set-attribute>" +
                                        "<object-name name=\":service=Scheduler\"/>" +
                                        "<attribute name=\"PeriodTime\">5000</attribute>" +
                                        "</set-attribute>" +
                                        "</jmx>"
                                        ).getBytes())
            );
         log.debug( "Call invokeXML with: " + lTest );
         
         server.invoke(lName,
                       "invokeXML",
                       new Object[] { lTest },
                       new String[] { Document.class.getName() });
      }
   }

   protected void stopService() throws Exception
   {
      InitialContext ctx = new InitialContext();

      try {
         ctx.unbind(JNDI_NAME);
      }
      finally {
         ctx.close();
      }
   }
}

