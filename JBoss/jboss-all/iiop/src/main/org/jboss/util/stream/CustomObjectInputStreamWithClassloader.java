/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.InputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectStreamClass;

import java.lang.reflect.Proxy;

/**
 * Customized object input stream that 
 * <ul>
 * <li> redefines <code>readClassDescriptor()</code> in order to read a short 
 *      class descriptor (just the class name) when deserializing an 
 *      object</li>
 * <li> takes a class loader in its constructor and uses it to retrieve 
 *      the class definitions.</li>
 * </ul>
 *
 * @author  <a href="mailto:rickard@dreambean.com">Rickard Oberg</a>
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class CustomObjectInputStreamWithClassloader
      extends ObjectInputStream 
{
   
   /**
    * The classloader to use when the default classloader cannot find
    * the classes in the stream.
    */
   ClassLoader cl;
   
   /**
    * Constructs a new instance with the given classloader and input stream.
    *
    * @param  in      stream to read objects from
    * @param  cl      classloader to use
    */
   public CustomObjectInputStreamWithClassloader(InputStream in, 
                                                 ClassLoader cl)
      throws IOException 
   {
      super(in);
      this.cl = cl;
   }
   
   /**
    * Reads just the class name from this input stream.
    *
    * @return a class description object
    */
   protected ObjectStreamClass readClassDescriptor()
      throws IOException, ClassNotFoundException
   {
      return ObjectStreamClass.lookup(cl.loadClass(readUTF()));
   }
   
   /**
    * Resolves the class described in the classdesc parameter. First, try the
    * default classloader (implemented by the super class). If it cannot
    * load the class, try the classloader given to this instance.
    *
    * @param       classdesc            class description object
    * @return      the Class corresponding to class description
    * @exception   IOException             if an I/O error occurs
    * @exception   ClassNotFoundException  if the class cannot be found 
    *                                      by the classloader
    */
   protected Class resolveClass(ObjectStreamClass classdesc)
      throws IOException, ClassNotFoundException 
   {
      return cl.loadClass(classdesc.getName());
   }
   
   /**
    * Resolves the proxy class for the specified array of interfaces. 
    *
    * @param       interfaces              an array of interfaces
    * @return      the proxy class
    * @exception   IOException             if an I/O error occurs
    * @exception   ClassNotFoundException  if the class cannot be found 
    *                                      by the classloader
    */
   protected Class resolveProxyClass(String[] interfaces)
      throws IOException, ClassNotFoundException
   {
      
      Class[] interfacesClass = new Class[interfaces.length];
      for( int i=0; i< interfaces.length; i++ ) {
         interfacesClass[i] = Class.forName(interfaces[i], false, cl);
      }
      return Proxy.getProxyClass(cl, interfacesClass);
   }
}
