/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.modelmbean;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;

import org.jboss.mx.util.Serialization;

/**
 * Thrown when unrecognizable target object type is set to a Model MBean
 * instance
 *
 * @see javax.management.modelmbean.ModelMBean
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class InvalidTargetObjectTypeException
         extends Exception
{
   // Attributes ----------------------------------------------------
   private Exception exception = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = 3711724570458346634L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("msgStr",        String.class),
            new ObjectStreamField("relatedExcept", Exception.class),
         };
         break;
      default:
         serialVersionUID = 1190536278266811217L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("exception", Exception.class),
         };
      }
   }

   // Constructors --------------------------------------------------
   public InvalidTargetObjectTypeException()
   {
      super();
   }
   
   public InvalidTargetObjectTypeException(String s)
   {
      super(s);
   }
   
   public InvalidTargetObjectTypeException(Exception e, String s)
   {
      super(s);
      this.exception = e;
   }

   // Object overrides ----------------------------------------------

   /**
    * Returns a string representation of this exception. The returned string
    * contains this exception name, message and a string representation of the
    * target exception if it has been set.
    *
    * @return string representation of this exception
    */
   public String toString()
   {
      return "InvalidTargetObjectTypeException: " + getMessage() + 
        ((exception == null) ? "" : "\nCause: " + exception.toString());
   }

   // Private -------------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         ObjectInputStream.GetField getField = ois.readFields();
         exception = (Exception) getField.get("relatedExcept", null);
         break;
      default:
         ois.defaultReadObject();
      }
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         ObjectOutputStream.PutField putField = oos.putFields();
         putField.put("msgStr", getMessage());
         putField.put("relatedExcept", exception);
         oos.writeFields();
         break;
      default:
         oos.defaultWriteObject();
      }
   }
}




