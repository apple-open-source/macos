/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.invocation.iiop;

import java.io.IOException;
import java.io.Serializable;
import org.jboss.util.Conversion;

/**
 * Helper class used to create a byte array ("reference data") to be embedded
 * into a CORBA reference and to extract object/servant identification info
 * from this byte array.  If this info consists simply of an 
 * <code>objectId</code>, this id is serialized into the byte array. If this 
 * info consists of a pair (servantId, objectId), a <code>ReferenceData</code>
 * instance containing the pair is is serialized into the byte array. 
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class ReferenceData
      implements Serializable
{
   private Object servantId;
   private Object objectId;

   public static byte[] create(Object servantId, Object objectId)
   {
      return Conversion.toByteArray(new ReferenceData(servantId, objectId));
   }

   public static byte[] create(Object id)
   {
      return Conversion.toByteArray(id);
   }

   public static Object extractServantId(byte[] refData, ClassLoader cl) 
         throws IOException, ClassNotFoundException 
   {
      Object obj = Conversion.toObject(refData, cl);
      if (obj != null && obj instanceof ReferenceData)
         return ((ReferenceData)obj).servantId;
      else
         return obj;
   }

   public static Object extractServantId(byte[] refData) 
         throws IOException, ClassNotFoundException 
   {
      return extractServantId(refData, 
                              Thread.currentThread().getContextClassLoader());
   }

   public static Object extractObjectId(byte[] refData, ClassLoader cl) 
         throws IOException, ClassNotFoundException 
   {
      Object obj = Conversion.toObject(refData, cl);
      if (obj != null && obj instanceof ReferenceData)
         return ((ReferenceData)obj).objectId;
      else
         return obj;
   }
   
   public static Object extractObjectId(byte[] refData) 
         throws IOException, ClassNotFoundException 
   {
      return extractObjectId(refData, 
                             Thread.currentThread().getContextClassLoader());
   }
   
   private ReferenceData(Object servantId, Object objectId) 
   {
      this.servantId = servantId;
      this.objectId = objectId;
   }
   
}

