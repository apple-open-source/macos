/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.referenceable;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.lang.ref.Reference;

import java.rmi.MarshalledObject;

import javax.naming.RefAddr;

/**
 *  This class is used to store a serializable object in a RefAddr object.
 *
 * @author     Scott M Stark (Scott_Stark@d...)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class ObjectRefAddr extends RefAddr {

   private byte[]   serialContent;


   /**
    *  ObjectRefAddr constructor comment.
    *
    * @param  arg1                              java.lang.String
    * @param  content                           Description of Parameter
    * @exception  javax.naming.NamingException  Description of Exception
    */
   public ObjectRefAddr( String arg1, Object content )
      throws javax.naming.NamingException {
      super( arg1 );

      try {
         java.rmi.MarshalledObject mo = new MarshalledObject( content );
         ByteArrayOutputStream baos = new ByteArrayOutputStream();
         ObjectOutputStream oos = new ObjectOutputStream( baos );
         oos.writeObject( mo );
         serialContent = baos.toByteArray();
      } catch ( java.io.IOException e ) {
         e.printStackTrace();
         throw new javax.naming.NamingException( "Could not create a reference: " + e.getMessage() );
      }
   }

   /**
    *  getContent method comment.
    *
    * @return    The Content value
    */
   public Object getContent() {
      return serialContent;
   }

   /**
    *  getContent method comment.
    *
    * @param  ref                               Description of Parameter
    * @param  arg1                              Description of Parameter
    * @return                                   Description of the Returned
    *      Value
    * @exception  javax.naming.NamingException  Description of Exception
    */
   public static Object extractObjectRefFrom( javax.naming.Reference ref, String arg1 )
      throws javax.naming.NamingException {

      try {
         byte[] serialContent = ( byte[] )ref.get( arg1 ).getContent();
         ByteArrayInputStream bais = new ByteArrayInputStream( serialContent );
         ObjectInputStream ois = new ObjectInputStream( bais );
         java.rmi.MarshalledObject mo = ( java.rmi.MarshalledObject )ois.readObject();
         return mo.get();
      } catch ( Exception e ) {
         throw new javax.naming.NamingException( "Invalid reference.  Error: " + e.getMessage() );
      }

   }
}
