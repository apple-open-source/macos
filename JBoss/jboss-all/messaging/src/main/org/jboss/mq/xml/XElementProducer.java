/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.xml;

import java.util.Vector;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.DefaultHandler;

/**
 *  XElementProducer parses and provides an XElementConsumer XElement data. <p>
 *
 *  An XElementProducer is a SAX based XML parser. This class provides a hybrid
 *  of the Event Based and DOM based XML parsing models. This is done by telling
 *  the XElementProducer which elements signal the start of a record, and when
 *  to generat an XElement. As the "record" XML elements are encountered,
 *  XElements are produced. These XElements create an in memory DOM tree of the
 *  element and all sub elements. After the "record" element has be fully read
 *  in through the use of SAX events, the "record" element, now in the form of
 *  an XElement, is passed to an XElementConsumer for processing.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.5 $
 */
public class XElementProducer {

   private XElementConsumer consumer;
   private Vector   targetRecords = new Vector();
   private Handler  handler = new Handler();
   private Exception thrownError = null;

   /**
    *  The Constructor must be passed the consumer object.
    *
    * @param  consumerObject  the object that will process data produced from
    *      this object.
    */
   public XElementProducer( XElementConsumer consumerObject ) {
      consumer = consumerObject;
   }

   /**
    *  Adds a name to the list of element names which when encountered, will
    *  produce an XElement record.
    *
    * @param  name
    */
   public void addElementRecord( String name ) {
      targetRecords.addElement( name );
   }

   /**
    *  Clears all the previously added element record names.
    */
   public void clearElementRecords() {
      targetRecords.removeAllElements();
   }

   /**
    *  Starts parsing a file. As "record" elements are encountered, record
    *  XElements are produced and given to the XElementConsumer to process.
    *
    * @param  is          Description of Parameter
    * @throws  Exception  includes IO errors, parse errors, or Exceptions thrown
    *      from the RecordConsumer.
    */
   public void parse( java.io.InputStream is )
      throws Exception {
      if ( consumer == null ) {
         throw new NullPointerException();
      }
      try {

         SAXParserFactory factory = SAXParserFactory.newInstance();
         SAXParser parser = factory.newSAXParser();

         if ( consumer instanceof org.xml.sax.ErrorHandler ) {
            XMLReader reader = parser.getXMLReader();
            reader.setErrorHandler( ( org.xml.sax.ErrorHandler )consumer );
         }
         thrownError = null;
         parser.parse( new InputSource( is ), handler );
      } catch ( SAXException e ) {
         if ( thrownError != null ) {
            throw thrownError;
         } else {
            throw e;
         }
      }
   }

   /**
    *  Starts parsing a file. As "record" elements are encountered, record
    *  XElements are produced and given to the XElementConsumer to process.
    *
    * @param  url         Description of Parameter
    * @throws  Exception  includes IO errors, parse errors, or Exceptions thrown
    *      from the RecordConsumer.
    */
   public void parse( java.net.URL url )
      throws Exception {
      if ( consumer == null ) {
         throw new NullPointerException();
      }
      try {
         SAXParserFactory factory = SAXParserFactory.newInstance();
         SAXParser parser = factory.newSAXParser();

         if ( consumer instanceof org.xml.sax.ErrorHandler ) {
            XMLReader reader = parser.getXMLReader();
            reader.setErrorHandler( ( org.xml.sax.ErrorHandler )consumer );
         }
         thrownError = null;
         parser.parse( url.toExternalForm(), handler );
      } catch ( SAXException e ) {
         if ( thrownError != null ) {
            throw thrownError;
         } else {
            throw e;
         }
      }
   }

   //
   // INNER CLASS : Handler
   // Used to handle for SAX events.
   //
   // If we get an element whose name is in targetRecords vector then
   // we start a new root XElement and set currentXElement to this object.
   // currentXElement is always the top element that is being processed.
   // (He's working like the top pointer of a stack)
   //
   // As sub elements are encountered, they will be added to the currentXElement
   // and then they become the currentXElement.
   //
   // When the end of an element is encountered, then currentXElement
   // is set to the parent of currentXElement.
   //
   // Exception processing is a little trick, read on:
   // An XElementConsumer is allowed to throw any kind of exception
   // when processing records.  But since the SAX event based parser only allows
   // you to throw SAXExceptions from it's event handler methods, this class
   // uses the thrownError variable to store the thrown event.  A SAXException
   // is then generated to stop the SAXParser and the XElementProducer.parse()
   // method checks to see if the thrownError variable was set, if so, then
   // it throws the exception stored in thrownError.
   //
   /**
    * @created    August 16, 2001
    */
   class Handler extends DefaultHandler {
      private XElement currentXElement;

      public void startDocument()
         throws SAXException {
         try {
            consumer.documentStartEvent();
         } catch ( Exception e ) {
            thrownError = e;
            throw new SAXException( e.toString() );
         }
      }

      public void endDocument()
         throws SAXException {
         try {
            consumer.documentEndEvent();
         } catch ( Exception e ) {
            thrownError = e;
            throw new SAXException( e.toString() );
         }
      }

      public void startElement( String uri, String localName, String qname, Attributes atts )
         throws SAXException {
         if ( currentXElement != null ) {
            XElement o = new XElement( qname, atts );
            currentXElement.addElement( o );
            currentXElement = o;
         } else {
            if ( targetRecords.size() == 0 ) {
               currentXElement = new XElement( qname, atts );
            } else {
               for ( int i = 0; i < targetRecords.size(); i++ ) {
                  if ( qname.equals( targetRecords.elementAt( i ) ) ) {
                     currentXElement = new XElement( qname, atts );
                     break;
                  }
               }
            }
         }
      }

      public void endElement( String uri, String localName, String qName )
         throws SAXException {
         if ( currentXElement != null ) {
            // Sanity Check :
            if ( !qName.equals( currentXElement.getName() ) ) {
               throw new SAXException( "XElement parsing sanitity check failed" );
            }
            XElement t = currentXElement;
            currentXElement = currentXElement.getParent();
            if ( currentXElement == null ) {
               try {
                  consumer.recordReadEvent( t );
               } catch ( Exception e ) {
                  thrownError = e;
                  throw new SAXException( e.toString() );
               }
            }
         }
      }

      public void characters( char[] chars, int start, int length ) {
         if ( length == 0 ) {
            return;
         }
         if ( currentXElement != null ) {
            currentXElement.add( new String( chars, start, length ) );
         }
      }
   }

}
