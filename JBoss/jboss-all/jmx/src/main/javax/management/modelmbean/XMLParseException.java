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
 * Exceptions related to XML handling.
 *
 * @see javax.management.modelmbean.DescriptorSupport
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
 *   
 */
public class XMLParseException
         extends Exception
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = -7780049316655891976L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("msgStr", String.class)
         };
         break;
      default:
         serialVersionUID = 3176664577895105181L;
         serialPersistentFields = new ObjectStreamField[0];
      }
   }
   
   // Constructors --------------------------------------------------
   public XMLParseException()
   {
      super();
   }

   public XMLParseException(String s)
   {
      super(s);
   }

   public XMLParseException(Exception e, String s)
   {
      // REVIEW Adrian Brock: The exception doesn't seem to be serialized so
      // I'm including it in the constructed message.   
      super("XMLParseException: " + s + 
        ((e == null) ? "" : "\nCause: " + e.toString()));
   }

   // Object overrides ----------------------------------------------

   // Private -------------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
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
         oos.writeFields();
         break;
      default:
         oos.defaultWriteObject();
      }
   }
}




