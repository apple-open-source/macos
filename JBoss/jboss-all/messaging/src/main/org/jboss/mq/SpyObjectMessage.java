/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectStreamClass;
import java.io.Externalizable;
import java.io.Serializable;
import java.io.IOException;
import java.lang.reflect.Array;

import javax.jms.JMSException;
import javax.jms.MessageFormatException;
import javax.jms.MessageNotWriteableException;
import org.jboss.util.Classes;

import javax.jms.ObjectMessage;

/**
 *  This class implements javax.jms.ObjectMessage
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.13 $
 */
public class SpyObjectMessage
       extends SpyMessage
       implements ObjectMessage, Externalizable {

   // Attributes ----------------------------------------------------

   boolean          isByteArray = false;
   byte[]           objectBytes = null;

   private final static long serialVersionUID = 8809953915407712952L;

   // Public --------------------------------------------------------

   public void setObject( Serializable object )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "setObject" );
      }
      try {
         if ( object instanceof byte[] ) {
            //cheat for byte arrays
            isByteArray = true;
            objectBytes = new byte[( ( byte[] )object ).length];
            System.arraycopy( object, 0, objectBytes, 0, objectBytes.length );
         } else {
            isByteArray = false;
            ByteArrayOutputStream byteArray = new ByteArrayOutputStream();
            ObjectOutputStream objectOut = new ObjectOutputStream( byteArray );
            objectOut.writeObject( object );
            objectBytes = byteArray.toByteArray();
            objectOut.close();
         }
      } catch ( IOException e ) {
         throw new MessageFormatException( "Object cannot be serialized" );
      }
   }

   public Serializable getObject()
      throws JMSException {

      Serializable retVal = null;
      try {
         if ( null != objectBytes ) {
            if ( isByteArray ) {
               retVal = new byte[objectBytes.length];
               System.arraycopy( objectBytes, 0, retVal, 0, objectBytes.length );
            } else {

               /**
               * Default implementation ObjectInputStream does not work well
               * when running an a micro kernal style app-server like JBoss.
               * We need to look for the Class in the context class loader
               * and not in the System classloader.
               *
               * Would this be done better by using a MarshaedObject??
               */
               class ObjectInputStreamExt extends ObjectInputStream
               {
                  ObjectInputStreamExt(InputStream is) throws IOException
                  {
                     super(is);
                  }
                  
                  protected Class resolveClass(ObjectStreamClass v) throws IOException, ClassNotFoundException
                  {
                     return Classes.loadClass(v.getName());
                  }
               }
               ObjectInputStream input = new ObjectInputStreamExt(new ByteArrayInputStream(objectBytes));
               retVal = (Serializable) input.readObject();
               input.close();
            }         
         }
      } catch ( ClassNotFoundException e ) {
         throw new MessageFormatException( "ClassNotFoundException: "+e.getMessage() );
      } catch ( IOException e ) {
         throw new MessageFormatException( "IOException: "+e.getMessage() );
      }
      return retVal;
   }

   public void clearBody()
      throws JMSException {
      objectBytes = null;
      super.clearBody();
   }

   public SpyMessage myClone()
      throws JMSException {
      SpyObjectMessage result = MessagePool.getObjectMessage();
      result.copyProps( this );
      result.isByteArray = this.isByteArray;
      if ( objectBytes != null ) {
         result.objectBytes = new byte[this.objectBytes.length];
         System.arraycopy( this.objectBytes, 0, result.objectBytes, 0, this.objectBytes.length );
      }
      return result;
   }

   public void writeExternal( java.io.ObjectOutput out )
      throws java.io.IOException {
      super.writeExternal( out );
      out.writeBoolean( isByteArray );
      if ( objectBytes == null ) {
         out.writeInt( -1 );
      } else {
         out.writeInt( objectBytes.length );
         out.write( objectBytes );
      }
   }

   public void readExternal( java.io.ObjectInput in )
      throws java.io.IOException, ClassNotFoundException {
      super.readExternal( in );
      isByteArray = in.readBoolean();
      int length = in.readInt();
      if ( length < 0 ) {
         objectBytes = null;
      } else {
         objectBytes = new byte[length];
         in.readFully( objectBytes );
      }
   }

}
/*
vim:tabstop=3:et:shiftwidth=3
*/
