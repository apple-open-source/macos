/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamClass;
import java.io.OutputStream;

/**
 * Customized object output stream that redefines 
 * <code>writeClassDescriptor()</code> in order to write a short class 
 * descriptor (just the class name) when serializing an object.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class CustomObjectOutputStream
      extends ObjectOutputStream 
{
   
   /**
    * Constructs a new instance with the given output stream.
    *
    * @param out     stream to write objects to
    */
   public CustomObjectOutputStream(OutputStream out)
      throws IOException 
   {
      super(out);
   }
   
   /**
    * Writes just the class name to this output stream.
    *
    * @param classdesc class description object
    */
   protected void writeClassDescriptor(ObjectStreamClass classdesc)
      throws IOException 
   {
      writeUTF(classdesc.getName());
   }
   
}
