package org.jboss.web.tomcat.tc4;

import org.xml.sax.Attributes;
import org.xml.sax.helpers.AttributesImpl;
import org.xml.sax.ContentHandler;
import org.xml.sax.SAXException;

//import org.apache.catalina.util.xml.XmlMapper;
import org.apache.commons.digester.Digester;

import org.jboss.logging.Logger;
import org.jboss.util.Strings;

/** LoggedXmlMapper : a catalina XmlMapper that log onto a jboss Logger.
 This actually helps very little since the XmlMapper uses System.out.println
 directly in numerous places.

 TOMCAT 4.1.12 UPDATE: Extends org.apache.jakarta.commons.Digester instead
 of XmlMapper. Sundry related changes for setting up Rules, etc.


@author Scott.Stark@jboss.org
@author alain.coetmeur@caissedesdepots.fr
 */
public class LoggedXmlMapper extends Digester
   implements ContentHandler
{
   protected Logger log;
   
   public LoggedXmlMapper()
   {
      this.log = Logger.getLogger(getClass());
   }
   public LoggedXmlMapper(Logger log)
   {
      this.log = log;
   }
   public LoggedXmlMapper(String category)
   {
      this.log = Logger.getLogger(category);
   }

   public void log(String msg)
   {
      if(log.isTraceEnabled())
      {
         log.trace(msg);
      }
      else if(log.isDebugEnabled())
      {
         log.debug(msg);
      }
   }

// Begin ContentHandler interface methods
   public void startElement(String namespaceURI, String localName, String qName,
      Attributes attributes) throws SAXException
   {
      log.trace("startElement <"+localName+">");
      AttributesImpl newAttributes = new AttributesImpl();
      int count = attributes.getLength();
      for(int a = 0; a < count; a ++)
      {
         String value = attributes.getValue(a);
         value = Strings.replaceProperties(value);
         newAttributes.addAttribute(attributes.getURI(a),
            attributes.getLocalName(a), attributes.getQName(a),
            attributes.getType(a), value);
      }
      super.startElement(namespaceURI, localName, qName, newAttributes);
   }
   public void endElement(String namespaceURI, String localName, String qName) throws SAXException
   {
      log.trace("endElement <"+localName+">");
      super.endElement(namespaceURI, localName, qName);
   }

   public void startPrefixMapping(String prefix, String uri) throws SAXException
   {
   }
   public void endPrefixMapping(String prefix) throws SAXException
   {
   }   
   public void skippedEntity(String name) throws SAXException
   {
   }
// End ContentHandler interface methods

}
