/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.iiop.rmi.marshal;

import org.omg.CORBA_2_3.portable.OutputStream;

/**
 * Interface of an object that knows how to marshal a Java basic type or
 * object into a CDR input stream. Implementations of this interface are 
 * specialized for particular types: an <code>IntWriter</code> is a 
 * <code>CDRStreamWriter</code> that knows how to marshal <code>int</code>s,
 * a <code>LongWriter</code> is a <code>CDRStreamWriter</code> that knows how 
 * to marshal <code>long</code>s, and so on.  
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public interface CDRStreamWriter 
{
   /**
    * Marshals a Java basic data type or object into a CDR output stream.
    *
    * @param out the output stream
    * @param obj the basic data type (within a suitable wrapper instance)
    *            or object to be marshalled
    */
   void write(OutputStream out, Object obj);
}
