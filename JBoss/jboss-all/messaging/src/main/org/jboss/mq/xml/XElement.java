/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.xml;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Iterator;

import java.util.Vector;

import org.xml.sax.Attributes;

/**
 *  XElement provides an interface to an XML element. An XElement represents an
 *  XML element which contains: <br>
 *
 *  <ul>
 *    <li> Name (required)
 *    <li> Attributes (optional)
 *    <li> character data (optional)
 *    <li> other elements (optional)
 *  </ul>
 *  <p>
 *
 *  It is important to understand the diffrence between an "field" XElement and
 *  a non "field" XElement. If an XElement does not contain any sub elements, it
 *  is considered a "field" XElement. The <code>getField(String)</code> and
 *  <code>getValue()</code> functions will throw an XElementException if they
 *  are used on non "attribute" objects. This give you a little bit type
 *  checking (You'll get an exception if you try to access the character data of
 *  an element that has sub elements). <p>
 *
 *  If XElement is not an field, then it contains other XElements and optionaly
 *  some text. The text data can be accessed with the <code>getText()</code>
 *  method and the sub elements with the <code>iterator()</code> or with <code>
 *  getElementXXX()</code> fuctions. Since XML and thus XElements provide a tree
 *  type data structure, traversing the tree to access leaf data can be
 *  cumbersom if you have a 'deep' tree. For example, you may have to do: <code>
 *  element.getElement("tree").getElement("branch").getElement("leaf")</code>
 *  access a XElement 3 levels deep in the tree. To access deep elements easier,
 *  XElements lets you use 'reletive' names to access deep elements. Using
 *  reletive names, you could access the same element in previous example doing:
 *  <code>element.getElement("tree/branch/leaf")</code> When using relative
 *  names, keep in mind that "." will get the current XElement, and ".." will
 *  get parent XElement. Very similar to how URLs work.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.7 $
 */
public class XElement {

   private XElement parent = null;
   private String   name = null;
   private Hashtable metadata = new Hashtable();
   private Vector   contents = new Vector();
   private final static String nl = System.getProperty( "line.separator" );

   /**
    *  Constructs an empty object.
    *
    * @param  objectName  the tag or element name that this object represents.
    */
   public XElement( String objectName ) {
      if ( objectName == null ) {
         throw new NullPointerException();
      }
      name = objectName;
      contents.addElement( new StringBuffer() );
   }

   /**
    *  Constructs an XElement with it's parent and metatags set.
    *
    * @param  objectName  the tag or element name that this object represents.
    * @param  atts        Description of Parameter
    */
   public XElement( String objectName, Attributes atts ) {
      if ( objectName == null ) {
         throw new NullPointerException();
      }
      if ( atts == null ) {
         throw new NullPointerException();
      }
      name = objectName;
      contents.addElement( new StringBuffer() );
      for ( int i = 0; i < atts.getLength(); i++ ) {
         metadata.put( atts.getQName( i ), atts.getValue( i ) );
         //metadata.put( atts.getLocalName( i ), atts.getValue( i ) );
      }
   }

   /**
    *  Sets/Adds a metatag value Only metatags whose value is not empty will
    *  display when the <code>toString()</code> methods is called.
    *
    * @param  key    the name of the metatag
    * @param  value  the value to set the metatag to.
    */
   public void setAttribute( String key, String value ) {
      metadata.put( key, value );
   }

   /**
    *  Sets the object name
    *
    * @param  newName
    */
   public void setName( String newName ) {
      name = newName;
   }

   /**
    *  Gets the character data that was within this object. This fuction can
    *  only be used on objects that are attributes.
    *
    * @param  value               The new Value value
    * @returns                    the character data contained within this
    *      object.
    * @throws  XElementException  if the object was not an attribute object
    */
   public void setValue( String value )
      throws XElementException {
      if ( !isField() ) {
         throw new XElementException( "" + getName() + " is not an attribute object" );
      }
      contents.setElementAt( new StringBuffer( value ), 0 );
   }


   /**
    *  Sets/Adds a attribute
    *
    * @param  key                    the name of the attribute element
    * @param  value                  the value to set the attribute to.
    * @exception  XElementException  Description of Exception
    */
   public void setField( String key, String value )
      throws XElementException {
      getElement( key ).setValue( value );
   }

   /**
    *  Returns the value of a meta data value.
    *
    * @param  key  Description of Parameter
    * @return      The Attribute value
    * @returns     the value of the metadata item, or "" if the item has not
    *      been set.
    */
   public String getAttribute( String key ) {
      String t = ( String )metadata.get( key );
      if ( t == null ) {
         return "";
      }
      return t;
   }

   /**
    *  Returns the element name (tag name) of this XElement
    *
    * @return     The Name value
    * @returns
    */
   public java.lang.String getName() {
      return name;
   }

   /**
    *  Get the parent of this object, or the object the contains this one.
    *
    * @return     The Parent value
    * @returns    null if this object is not contained by any other XElement.
    */
   public XElement getParent() {
      return parent;
   }

   /**
    *  Gets the TRIMMED character data that was within this object. This differs
    *  from getValue() in that:
    *  <UL>
    *    <LI> this fuction will work on attribute and non attribute XElements
    *
    *    <LI> it will trim both ends of the character data before returning it.
    *
    *  </UL>
    *
    *
    * @return     The Text value
    * @returns    the character data contained within this object.
    */
   public String getText() {
      return contents.elementAt( 0 ).toString().trim();
   }

   /**
    *  Gets the character data that was within this object. This fuction can
    *  only be used on objects that are attributes.
    *
    * @return                     The Value value
    * @returns                    the character data contained within this
    *      object.
    * @throws  XElementException  if the object was not an attribute object
    */
   public String getValue()
      throws XElementException {
      if ( !isField() ) {
         throw new XElementException( "" + getName() + " is not an attribute object" );
      }
      return contents.elementAt( 0 ).toString();
   }

   /**
    *  Returns the first object contained in this object named relativeName.
    *
    * @param  relativeName        The name of the object to find
    * @return                     The Element value
    * @returns                    the XElement named relativeName
    * @throws  XElementException  if the object could not be found.
    */
   public XElement getElement( String relativeName )
      throws XElementException {
      if ( relativeName == null ) {
         throw new NullPointerException();
      }

      String names[] = {null, relativeName};

      // Does the name have a "/" in it?
      String split[] = splitFront( relativeName, "/" );
      if ( split != null ) {

         // was it an absolute name? (started with a '/')
         if ( split[0].length() == 0 ) {
            // we are the parent
            if ( parent == null ) {
               split[0] = null;
            }
            // Let my parent handle the request.
            else {
               return parent.getElement( relativeName );
            }
         }

         // did we have a trailing / in the name?
         if ( split[1].length() == 0 ) {
            // For the case of "/",
            if ( split[0].equals( null ) ) {
               return this;
            }

            //otherwise it is an error
            // to leave a trailing /, for example tree/leaf/
            throw new XElementException( "Invalid name (trailing '/') : " + relativeName );
         }

         names = split;
      }

      if ( names[0] == null ) {
         for ( int i = 1; i < contents.size(); i++ ) {
            XElement o = ( XElement )contents.elementAt( i );
            if ( names[1].equals( o.getName() ) ) {
               return o;
            }
         }
      } else {
         if ( names[0].equals( "." ) ) {
            return getElement( names[1] );
         } else if ( names[0].equals( ".." ) ) {
            if ( parent != null ) {
               return parent.getElement( names[1] );
            } else {
               throw new XElementException( "Invalid name (" + getName() + " has no parent) : " + relativeName );
            }
         } else {
            for ( int i = 1; i < contents.size(); i++ ) {
               XElement o = ( XElement )contents.elementAt( i );
               if ( names[0].equals( o.getName() ) ) {
                  return o.getElement( names[1] );
               }
            }
         }
      }

      throw new XElementException( "Invalid name (" + getName() + " does not contain the name) : " + relativeName );
   }


   /**
    *  Gets the value of a contained attribute object.
    *
    * @param  objectName          The name of the attribute object.
    * @return                     The Field value
    * @returns                    the value of the attribute object.
    * @throws  XElementException  if the object does not exist or if its not an
    *      attribute object.
    */
   public String getField( String objectName )
      throws XElementException {
      return getElement( objectName ).getValue();
   }

   /**
    *  Returns true if the object is an attribute object. An object is an
    *  attribute object if it does not contain any other objects.
    *
    * @return     The Field value
    * @returns    true if the object is an attribute object.
    */
   public boolean isField() {
      return contents.size() == 1;
   }

   /**
    *  Returns all the contained objects with the specified name.
    *
    * @param  relativeName  The name of the objects
    * @return               The ElementsNamed value
    * @returns              whose name is relativeName;
    */
   public java.util.Enumeration getElementsNamed( String relativeName ) {

      Vector t = new Vector();
      addElementsToVector( t, relativeName );
      return t.elements();
   }

   /**
    *  Adds and appends string data to the objects text.
    *
    * @param  data  Description of Parameter
    */
   public void add( String data ) {
      ( ( StringBuffer )contents.elementAt( 0 ) ).append( data );
   }

   /**
    *  Serializes this object into a string.
    *
    * @return    Description of the Returned Value
    */
   public String toString() {
      return toString( 0, true );
   }

   /**
    *  Adds an XElement to the set of XElements that are contained by this
    *  object.
    *
    * @param  subObject
    */
   public void addElement( XElement subObject ) {
      contents.addElement( subObject );
      subObject.parent = this;
   }

   /**
    *  Adds an XElement to the set of XElements that are contained by this
    *  object.
    *
    * @param  key    The feature to be added to the Field attribute
    * @param  value  The feature to be added to the Field attribute
    */
   public void addField( String key, String value ) {
      XElement subObject = new XElement( key );
      subObject.add( value );
      addElement( subObject );
   }

   /**
    *  Tests to see if this object contains the specified object.
    *
    * @param  objectName  The name of the object.
    * @return             Description of the Returned Value
    * @returns            true if the object exits.
    */
   public boolean containsElement( String objectName ) {
      try {
         getElement( objectName );
         return true;
      } catch ( XElementException e ) {
         return false;
      }
   }

   /**
    *  Tests to see if this object contains the specified attribute object.
    *
    * @param  objectName  The name of the attribute object.
    * @return             Description of the Returned Value
    * @returns            true if the attribute exits.
    */
   public boolean containsField( String objectName ) {
      try {
         XElement obj = getElement( objectName );
         return obj.isField();
      } catch ( XElementException e ) {
         return false;
      }
   }

   /**
    *  Serializes this object into a string.
    *
    * @param  nestingLevel  how many tabs to prepend to output
    * @param  indent        Description of Parameter
    * @return               Description of the Returned Value
    */
   public String toString( int nestingLevel, boolean indent ) {
      try {
         StringBuffer indentation = new StringBuffer();
         StringBuffer rc = new StringBuffer();
         if ( indent ) {
            for ( int i = 0; i < nestingLevel; i++ ) {
               indentation.append( '\t' );
            }
         }
         rc.append( indentation.toString() );
         rc.append( "<" );
         rc.append( getName() );
         Enumeration enum = metadata.keys();
         while ( enum.hasMoreElements() ) {
            String key = ( String )enum.nextElement();
            String value = ( String )metadata.get( key );
            rc.append( ' ' );
            rc.append( key );
            rc.append( "=\"" );
            rc.append( metaValueEncode( value ) );
            rc.append( '"' );
         }
         if ( isField() ) {
            if ( getValue().length() == 0 ) {
               rc.append( "/>" );
               rc.append( nl );
            } else {
               rc.append( '>' );
               rc.append( valueEncode( getValue() ) );
               rc.append( "</" );
               rc.append( getName() );
               rc.append( '>' );
               rc.append( nl );
            }
         } else {
            rc.append( '>' );
            rc.append( nl );
            String text = getText();
            if ( text.length() > 0 ) {
               rc.append( indentation.toString() + "\t" );
               rc.append( getText() );
               rc.append( nl );
            }
            for ( int i = 1; i < contents.size(); i++ ) {
               Object o = contents.elementAt( i );
               rc.append( ( ( XElement )o ).toString( nestingLevel + 1, indent ) );
            }
            rc.append( indentation.toString() );
            rc.append( "</" );
            rc.append( getName() );
            rc.append( '>' );
            rc.append( nl );
         }
         return rc.toString();
      } catch ( XElementException e ) {
         // This should not occur!
         e.printStackTrace();
         System.exit( 1 );
         return "";
      }
   }

   /**
    *  Serializes this object into a XML document String.
    *
    * @param  indent  Description of Parameter
    * @return         Description of the Returned Value
    */
   public String toXML( boolean indent ) {
      return
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + nl +
            toString( 0, indent );
   }

   /**
    *  Removes this XElement from the parent.
    *
    * @throws  XElementException  if the object did not have a parent
    */
   public void removeFromParent()
      throws XElementException {
      if ( parent == null ) {
         throw new XElementException( "" + getName() + " does not have a parent" );
      }

      parent.contents.remove( this );
      parent = null;
   }

   /**
    * @return     Description of the Returned Value
    * @returns    an Enumeration of all the XElement conatained within this
    *      object.
    */
   public Enumeration elements() {
      return getElementsNamed( "*" );
   }

   /**
    *  adds all the contains elements to the vector that match the relative
    *  name.
    *
    * @param  t             The feature to be added to the ElementsToVector
    *      attribute
    * @param  relativeName  The feature to be added to the ElementsToVector
    *      attribute
    */
   private void addElementsToVector( Vector t, String relativeName ) {

      String names[] = {null, relativeName};

      // Does the name have a "/" in it?
      String split[] = splitFront( relativeName, "/" );
      if ( split != null ) {

         // was it an absolute name? (started with a '/')
         if ( split[0].length() == 0 ) {
            // we are the parent
            if ( parent == null ) {
               split[0] = null;
            } else {
               // Let my parent handle the request.
               parent.addElementsToVector( t, relativeName );
               return;
            }
         }

         // did we have a trailing / in the name?
         if ( split[1].length() == 0 ) {
            return;
         }
         names = split;
      }

      if ( names[0] == null ) {
         if ( names[1].equals( "*" ) ) {
            for ( int i = 1; i < contents.size(); i++ ) {
               t.addElement( contents.elementAt( i ) );
            }
         } else {
            for ( int i = 1; i < contents.size(); i++ ) {
               XElement o = ( XElement )contents.elementAt( i );
               if ( names[1].equals( o.getName() ) ) {
                  t.addElement( o );
               }
            }
         }
      } else {
         if ( names[0].equals( "." ) ) {
            addElementsToVector( t, names[1] );
            return;
         } else if ( names[0].equals( ".." ) ) {
            if ( parent != null ) {
               parent.addElementsToVector( t, names[1] );
            }
            return;
         } else {
            for ( int i = 1; i < contents.size(); i++ ) {
               XElement o = ( XElement )contents.elementAt( i );
               if ( names[0].equals( o.getName() ) ) {
                  o.addElementsToVector( t, names[1] );
               }
            }
         }
      }
   }

   /**
    *  Constructs an empty object.
    *
    * @param  is                       Description of Parameter
    * @return                          Description of the Returned Value
    * @exception  XElementException    Description of Exception
    * @exception  java.io.IOException  Description of Exception
    */
   public static XElement createFrom( java.io.InputStream is )
      throws XElementException, java.io.IOException {
class MyRecordConsumer implements XElementConsumer {

         XElement   root = null;

         public void documentEndEvent() {
         }

         public void documentStartEvent() {
         }

         public void recordReadEvent( XElement o ) {
            root = o;
         }
      }

      MyRecordConsumer consumer = new MyRecordConsumer();
      XElementProducer producer = new XElementProducer( consumer );

      try {
         producer.parse( is );
         if ( consumer.root == null ) {
            throw new XElementException( "No root element" );
         }
         return consumer.root;
      } catch ( java.io.IOException e ) {
         throw e;
      } catch ( Exception e ) {
         throw new XElementException( "Parse Error: " + e );
      }
   }

   /**
    *  Constructs an empty object.
    *
    * @param  url                      Description of Parameter
    * @return                          Description of the Returned Value
    * @exception  XElementException    Description of Exception
    * @exception  java.io.IOException  Description of Exception
    */
   public static XElement createFrom( java.net.URL url )
      throws XElementException, java.io.IOException {
class MyRecordConsumer implements XElementConsumer {

         XElement   root = null;

         public void documentEndEvent() {
         }

         public void documentStartEvent() {
         }

         public void recordReadEvent( XElement o ) {
            root = o;
         }
      }

      MyRecordConsumer consumer = new MyRecordConsumer();
      XElementProducer producer = new XElementProducer( consumer );

      try {
         producer.parse( url );
         if ( consumer.root == null ) {
            throw new XElementException( "No root element" );
         }
         return consumer.root;
      } catch ( java.io.IOException e ) {
         throw e;
      } catch ( Exception e ) {
         throw new XElementException( "Parse Error: " + e );
      }
   }


   private static String findAndReplace( String value, String searchStr, String replaceStr ) {
      StringBuffer buffer = new StringBuffer( value.length() );
      while ( value.length() > 0 ) {
         int pos = value.indexOf( searchStr );
         if ( pos != -1 ) {
            buffer.append( value.substring( 0, pos ) );
            buffer.append( replaceStr );
            if ( pos + searchStr.length() < value.length() ) {
               value = value.substring( pos + searchStr.length() );
            } else {
               value = "";
            }
         } else {
            buffer.append( value );
            value = "";
         }
      }
      return buffer.toString();
   }

   private static String metaValueEncode( String value ) {
      value = findAndReplace( value, "&", "&amp;" );
      value = findAndReplace( value, "\"", "&quot;" );
      value = findAndReplace( value, "'", "&apos;" );
      return utf8Encode( value );
   }


   private static String utf8Encode( String value ) {
      try {
         char buff[] = new char[value.length()];
         value.getChars( 0, buff.length, buff, 0 );
         sun.io.CharToByteUTF8 conv = new sun.io.CharToByteUTF8();
         byte b[] = conv.convertAll( buff );
         return new String( b );
      } catch ( sun.io.MalformedInputException e ) {
         return null;
      }
   }

   private static String valueEncode( String value ) {
      value = findAndReplace( value, "&", "&amp;" );
      value = findAndReplace( value, "<", "&lt;" );
      value = findAndReplace( value, ">", "&gt;" );
      return utf8Encode( value );
   }


   private static String[] splitFront( String string, String splitMarker ) {

      if ( string == null || splitMarker == null ) {
         throw new NullPointerException();
      }

      String front;
      String back;

      int pos = string.indexOf( splitMarker );
      if ( pos == -1 ) {
         return null;
      }

      int l = splitMarker.length();
      front = string.substring( 0, pos );
      if ( pos + l >= string.length() ) {
         back = "";
      } else {
         back = string.substring( pos + l );
      }

      String rc[] = {front, back};
      return rc;
   }
   
   public String getOptionalField( String field) throws XElementException {
      if ( !containsField(field) ) 
         return null;
      return getField(field);
   }

   public void setOptionalField( String field, String value) throws XElementException {
      if ( value == null ) {
         if( containsField(field) )
         	  getElement(field).removeFromParent();
         return;
      }
      if( containsField(field) )
      	setField(field, value);
      else
      	addField(field, value);
   }
   
}
