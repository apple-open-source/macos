/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.IOException;

import org.jboss.util.stream.CustomObjectInputStreamWithClassloader;
import org.jboss.util.stream.CustomObjectOutputStream;

/**
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class Conversion
{
   /**
    * Receives an object and converts it into a byte array. Used to embed
    * a JBoss oid into the "reference data" (object id) field of a CORBA 
    * reference.
    */
   public static byte[] toByteArray(Object obj) 
   {
      try {
         ByteArrayOutputStream os = new ByteArrayOutputStream();
         ObjectOutputStream oos = new CustomObjectOutputStream(os);

         oos.writeObject(obj);
         oos.flush();
         byte[] a = os.toByteArray();
         os.close();
         return a;
      }
      catch (IOException ioe) {
         throw new RuntimeException("Object id serialization error:\n" + ioe);
      }
   }

   /**
    * Receives a classloader and a byte array previously returned by a call to 
    * <code>toByteArray</code> and retrieves an object from it. Used to 
    * extract a JBoss oid from the "reference data" (object id) field of a
    * CORBA reference. 
    */
   public static Object toObject(byte[] a, ClassLoader cl)
         throws IOException, ClassNotFoundException 
   {
      ByteArrayInputStream is = new ByteArrayInputStream(a);
      ObjectInputStream ois = 
	 new CustomObjectInputStreamWithClassloader(is, cl);
      Object obj = ois.readObject();
      is.close();
      return obj;
   }

   /**
    * Receives a byte array previously returned by a call to 
    * <code>toByteArray</code> and retrieves an object from it. Used to
    * extract a JBoss oid from the "reference data" (object id) field of a
    * CORBA reference. 
    */
   public static Object toObject(byte[] a) 
         throws IOException, ClassNotFoundException 
   {
      return toObject(a, Thread.currentThread().getContextClassLoader());
   }

}
