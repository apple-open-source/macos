package org.jnp.interfaces;

import java.io.IOException;
import java.io.Serializable;
import java.rmi.MarshalledObject;

/** An encapsulation of a JNDI binding as both the raw object and its
 MarshalledObject form. When accessed in the same VM as the JNP server,
 the raw object reference is used to avoid deserialization.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public class MarshalledValuePair implements Serializable
{
   public MarshalledObject marshalledValue;
   public transient Object value;

   /** Creates a new instance of MashalledValuePair */
   public MarshalledValuePair(Object value) throws IOException
   {
      this.value = value;
      this.marshalledValue = new MarshalledObject(value);
   }

   public Object get() throws ClassNotFoundException, IOException
   {
      Object theValue = value;
      if( theValue == null && marshalledValue != null )
         theValue = marshalledValue.get();
      return theValue;
   }
}
