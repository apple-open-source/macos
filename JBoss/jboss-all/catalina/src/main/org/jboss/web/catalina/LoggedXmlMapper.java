package org.jboss.web.catalina;

import org.xml.sax.Attributes;
import org.xml.sax.AttributeList;
import org.xml.sax.ContentHandler;
import org.xml.sax.SAXException;

import org.apache.catalina.util.xml.XmlMapper;

import org.jboss.logging.Logger;

/** LoggedXmlMapper : a catalina XmlMapper that log onto a jboss Logger.
 This actually helps very little since the XmlMapper uses System.out.println
 directly in numerous places.

@author Scott.Stark@jboss.org
@author alain.coetmeur@caissedesdepots.fr
 */
public class LoggedXmlMapper extends XmlMapper
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
      ToAttributeList attrList = new ToAttributeList(attributes);
      super.startElement(localName, attrList);
   }
   public void endElement(String namespaceURI, String localName, String qName) throws SAXException
   {
      log.trace("endElement <"+localName+">");
      super.endElement(localName);
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

   /** A converter from the SAX2 Attributes interface to the deprecated SAX1
    AttributeList interface used by the XmlMapper
    */
   static class ToAttributeList implements AttributeList
   {
      Attributes attributes;
      ToAttributeList(Attributes attributes)
      {
         this.attributes = attributes;
      }

      public int getLength()
      {
         return attributes.getLength();
      }

      public String getName(int i)
      {
         return attributes.getLocalName(i);
      }

      public String getType(int i)
      {
         return attributes.getType(i);
      }
      public String getType(String name)
      {
         return attributes.getType(name);
      }

      public String getValue(String name)
      {
         return attributes.getValue(name);
      }
      public String getValue(int i)
      {
         return attributes.getValue(i);
      }
   }
}
