/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.invocation;

import java.io.IOException;
import java.io.OutputStream;
import java.io.ObjectOutputStream;
import java.rmi.Remote;
import java.rmi.server.RemoteObject;
import java.rmi.server.RemoteStub;

/**
 * An ObjectOutputStream subclass used by the MarshalledValue class to
 * ensure the classes and proxies are loaded using the thread context
 * class loader. Currently this does not do anything as neither class or
 * proxy annotations are used.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.4 $
 */
public class MarshalledValueOutputStream
   extends ObjectOutputStream
{
   /** Creates a new instance of MarshalledValueOutputStream
    If there is a security manager installed, this method requires a
    SerializablePermission("enableSubstitution") permission to ensure it's
    ok to enable the stream to do replacement of objects in the stream.
    */
   public MarshalledValueOutputStream(OutputStream os) throws IOException
   {
      super(os);
      enableReplaceObject(true);
   }

   /**
    * @throws IOException   Any exception thrown by the underlying OutputStream.
    */
   protected void annotateClass(Class cl) throws IOException
   {
      super.annotateClass(cl);
   }
   
   /**
    * @throws IOException   Any exception thrown by the underlying OutputStream.
    */
   protected void annotateProxyClass(Class cl) throws IOException
   {
      super.annotateProxyClass(cl);
   }

   /** Override replaceObject to check for Remote objects that are
    not RemoteStubs.
   */
   protected Object replaceObject(Object obj) throws IOException
   {
      if( (obj instanceof Remote) && !(obj instanceof RemoteStub) )
      {
         Remote remote = (Remote) obj;
         try
         {
            obj = RemoteObject.toStub(remote);
         }
         catch(IOException ignore)
         {
            // Let the Serialization layer try with the orignal obj
         }
      }
      return obj;
   }
}
