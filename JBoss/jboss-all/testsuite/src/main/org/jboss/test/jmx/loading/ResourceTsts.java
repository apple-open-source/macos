/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.loading;

import java.io.InputStream;
import java.net.URL;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.apache.log4j.Category;
import org.jboss.system.ServiceMBeanSupport;
import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

/** This service access xml files as config resources from via the TCL
 *
 * @author Scott.Stark@jboss.org
 * @version  $Revision: 1.2 $
 */
public class ResourceTsts extends ServiceMBeanSupport implements ResourceTstsMBean
{
   static Category log = Category.getInstance(ResourceTsts.class);
   private String namespace = null;

   public ResourceTsts()
   {
      log.debug("ResourceTsts.ctor call stack", new Throwable("CallStack"));
   }

   public String getName()
   {
      return "ResourceTst";
   }

   public void setNamespace(String namespace)
   {
      this.namespace = namespace;
   }

   protected void startService() throws Exception
   {
      String serviceName = super.getServiceName().toString();
      log.debug("startService("+serviceName+")");
      log.debug("startService call stack", new Throwable("CallStack"));
      ClassLoader serviceLoader = getClass().getClassLoader();
      ClassLoader tcl = Thread.currentThread().getContextClassLoader();
      log.debug("ResourceTsts.CodeSource:"+getClass().getProtectionDomain().getCodeSource());
      log.debug("ResourceTsts.ClassLoader:"+serviceLoader);
      log.debug("ResourceTsts.startService() TCL:"+tcl);

      // Try some other resource names against the TCL
      URL url1 = tcl.getResource("META-INF/config.xml");
      log.debug("META-INF/config.xml via TCL: "+url1);
      URL url2 = tcl.getResource("/META-INF/config.xml");
      log.debug("/META-INF/config.xml via TCL: "+url2);
      URL url3 = tcl.getResource("file:/META-INF/config.xml");
      log.debug("file:/META-INF/config.xml via TCL: "+url3);
      URL url4 = tcl.getResource("META-INF/config.xml");
      log.debug("META-INF/config.xml via serviceLoader: "+url4);

      // Try loading via the TCL resource
      if( url1 == null )
         throw new IllegalStateException("No META-INF/config.xml available via TCL");
      InputStream is = url1.openStream();
      SAXParserFactory factory = SAXParserFactory.newInstance();
      SAXParser parser = factory.newSAXParser();
      ConfigHandler handler = new ConfigHandler(namespace);
      parser.parse(is, handler);
      log.debug("Successfully parsed url1");
      is.close();
      // Validate that the option matches our service name
      String optionValue = handler.value.toString();
      if( optionValue.equals(serviceName) )
         throw new IllegalStateException(optionValue+" != "+serviceName);
      log.debug("Config.option1 matches service name");
   }

   static class ConfigHandler extends DefaultHandler
   {
      boolean optionTag;
      StringBuffer value = new StringBuffer();
      String namespace;

      ConfigHandler(String namespace)
      {
         this.namespace = namespace;
      }
      public void startElement(String uri, String localName, String qName, Attributes attributes)
         throws SAXException
      {
         log.debug("startElement, uri="+uri+"localName="+localName+", qName="+qName);
         if( namespace == null )
            optionTag =  qName.equals("option1");
         else
            optionTag =  qName.equals(namespace+"option1");
      }
      public void characters(char[] str, int start, int length)
         throws SAXException
      {
         if( optionTag )
            value.append(str, start, length);
      }
      public void endElement(String uri, String localName, String qName)
         throws SAXException
      {
         log.debug("endElement, uri="+uri+"localName="+localName+", qName="+qName);
      }
   }
}
