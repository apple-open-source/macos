/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.iiop.rmi.marshal;

import org.omg.CORBA_2_3.portable.InputStream;

/**
 * Interface of an object that knows how to unmarshal a Java basic type or
 * object from a CDR input stream. Implementations of this interface are 
 * specialized for particular types: an <code>IntReader</code> is a 
 * <code>CDRStreamReader</code> that knows how to unmarshal <code>int</code>s,
 * a <code>LongReader</code> is a <code>CDRStreamReader</code> that knows how 
 * to unmarshal <code>long</code>s, and so on.  
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public interface CDRStreamReader 
{
   /**
    * Unmarshals a Java basic data type or object from a CDR input stream.
    *
    * @param in the input stream
    * @return   a basic data type (within a suitable wrapper instance) or
    *           object unmarshalled from the stream
    */
   Object read(InputStream in);
}
