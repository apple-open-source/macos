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
import java.io.StreamCorruptedException;

import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import java.util.Collections;

import java.io.Serializable;

import javax.management.Descriptor;
import javax.management.MBeanException;
import javax.management.RuntimeOperationsException;

import org.jboss.mx.util.Serialization;

/**
 * Support class for creating descriptors.
 *
 * @see javax.management.Descriptor
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 * @version $Revision: 1.6.4.1 $ 
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20020320 Juha Lindfors:</b>
 * <ul>
 * <li>toString() implementation</li>
 * </ul> 
 *
 * <p><b>20020525 Juha Lindfors:</b>
 * <ul>
 * <li>public currClass field removed to match JMX 1.1 MR </li>
 * </ul>
 *
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class DescriptorSupport
         implements Descriptor, Cloneable, Serializable
{

   // TODO: the spec doesn't define equality for descriptors
   //       we should override equals to match descriptor field, value pairs
   //       this does not appear to be the case with the 1.0 RI though
   
   
   // Attributes ----------------------------------------------------
   
   /**
    * Map for the descriptor field -> value.
    */
   private Map fieldMap = Collections.synchronizedMap(new HashMap(20));

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = 8071560848919417985L;
         break;
      default:
         serialVersionUID = -6292969195866300415L;
      }
      serialPersistentFields = new ObjectStreamField[]
      {
         new ObjectStreamField("descriptor", HashMap.class)
      };
   }

   
   // Constructors --------------------------------------------------
   /**
    * Default constructor.
    */
   public DescriptorSupport()
   {}

   /**
    * Creates descriptor instance with a given initial size.
    *
    * @param   initialSize initial size of the descriptor
    * @throws  MBeanException this exception is never thrown but is declared here
    *          for Sun RI API compatibility
    * @throws  RuntimeOperationsException if the <tt>initialSize<tt> is zero or negative. The target
    *          exception wrapped by this exception is an instace of <tt>IllegalArgumentException</tt> class.
    */
   public DescriptorSupport(int initialSize) throws MBeanException
   { 
      if (initialSize <= 0)
         // required by RI javadoc
         throw new RuntimeOperationsException(new IllegalArgumentException("initialSize <= 0"));
         
      fieldMap = Collections.synchronizedMap(new HashMap(initialSize));
   }

   /**
    * Copy constructor.
    *
    * @param   descriptor the descriptor to be copied
    * @throws  RuntimeOperationsException if descriptor is null. The target exception wrapped by this
    *          exception is an instance of <tt>IllegalArgumentException</tt> class.
    */
   public DescriptorSupport(DescriptorSupport descriptor)
   {
      //JPL: the javadoc says that if the descriptor parameter is null an empty copy will be created. It then
      //     goes on to say that if the descriptor paramter is null a RuntimeOperationsException will be thrown.
      //     Go figure.
      //
      //     RI accepts null argument so I guess we will too
      if (descriptor != null)
         this.setFields(descriptor.getFieldNames(), descriptor.getFieldValues(descriptor.getFieldNames()));
   }

   /**
    * Creates descriptor instance with given field names and values.if both field names and field
    * values array contain a <tt>null</tt> reference or empty arrays, an empty descriptor is created.
    * None of the name entries in the field names array can be a <tt>null</tt> reference.
    * Field values may contain <tt>null</tt> references.
    *
    * @param   fieldNames  Contains names for the descriptor fields. This array cannot contain
    *                      <tt>null</tt> references. If both <tt>fieldNames</tt> and <tt>fieldValues</tt>
    *                      arguments contain <tt>null</tt> or empty array references then an empty descriptor
    *                      is created. The size of the <tt>fieldNames</tt> array must match the size of
    *                      the <tt>fieldValues</tt> array.
    * @param   fieldValues Contains values for the descriptor fields. Null references are allowed.
    *
    * @throws RuntimeOperationsException if array sizes don't match
    */
   public DescriptorSupport(String[] fieldNames, Object[] fieldValues) throws RuntimeOperationsException
   {
      if (fieldNames == null && fieldValues == null)
         return;

      // FIXME: javadoc for setFields throws exception on null values as well         
      setFields(fieldNames, fieldValues);  
   }

   public DescriptorSupport(String[] fields)
   {
      // FIXME: implement the behavior in javadoc

      for (int i = 0; i < fields.length; ++i)
      {
         int index = fields[i].indexOf('=');
         if (index == -1)
            continue;

         String field = fields[i].substring(0, index);
         String value = fields[i].substring(index + 1, fields[i].length());

         fieldMap.put(field, value);
      }
   }

   // Public --------------------------------------------------------
   public Object getFieldValue(String inFieldName)
   {
      // FIXME: null or empty string
      return fieldMap.get(inFieldName);
   }

   public void setField(String inFieldName, Object fieldValue)
   {
      fieldMap.put(inFieldName, fieldValue);
   }

   public String[] getFields()
   {
      String[] fieldStrings = new String[fieldMap.size()];
      Iterator it = fieldMap.keySet().iterator();
      for (int i = 0; i < fieldMap.size(); ++i)
      {
         String key = (String)it.next();
         fieldStrings[i] = key + "=" + fieldMap.get(key);
      }

      return fieldStrings;
   }

   public String[] getFieldNames()
   {
      return (String[])fieldMap.keySet().toArray(new String[0]);
   }

   public Object[] getFieldValues(String[] fieldNames)
   {
      Object[] values = new Object[fieldNames.length];
      for (int i = 0; i < fieldNames.length; ++i)
         values[i] = fieldMap.get(fieldNames[i]);
         
      return values;
   }

   public void setFields(String[] fieldNames, Object[] fieldValues)
   {
      if (fieldNames == null || fieldValues == null)
         throw new RuntimeOperationsException(new IllegalArgumentException("fieldNames or fieldValues was null."));
         
      if (fieldNames.length == 0 || fieldValues.length == 0)
         return;
         
      if (fieldNames.length != fieldValues.length)
         throw new RuntimeOperationsException(new IllegalArgumentException("fieldNames and fieldValues array size must match."));
         
      for (int i = 0; i < fieldNames.length; ++i)
      {
         String name = fieldNames[i];
         if (name != null)
            fieldMap.put(name, fieldValues[i]);
      }
   }

   public synchronized Object clone()
   {
      try
      {
         DescriptorSupport clone = (DescriptorSupport)super.clone();
      
         // FIXME: should we clone the value objects in fieldMap?
         clone.fieldMap  = Collections.synchronizedMap(new HashMap(this.fieldMap));

         return clone;
      }
      catch (CloneNotSupportedException e)
      {
         // Descriptor interface won't allow me to throw CNSE
         throw new RuntimeOperationsException(new RuntimeException(e.getMessage()), e.toString());
      }
   }

   public void removeField(String fieldName)
   {
      fieldMap.remove(fieldName);
   }

   public boolean isValid() throws RuntimeOperationsException
   {
      // FIXME FIXME: basic validation for descriptors
      return true;
   }

   // Object overrides ----------------------------------------------
   public String toString()
   {
      String[] names  = getFieldNames();
      Object[] values = getFieldValues(names);
      
      if (names.length == 0)
         return "<empty descriptor>";
         
      StringBuffer sbuf = new StringBuffer(500);
      
      for (int i = 0; i < values.length; ++i)
      {
         sbuf.append(names[i]);
         sbuf.append("=");
         sbuf.append(values[i]);
         sbuf.append(",");
      }
      
      sbuf.deleteCharAt(sbuf.length() - 1);
      
      return sbuf.toString();
   }

   // Private -----------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      HashMap descriptor = (HashMap) getField.get("descriptor", null);
      if (descriptor == null)
         throw new StreamCorruptedException("Null descriptor?");
      fieldMap = Collections.synchronizedMap(descriptor);
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      putField.put("descriptor", new HashMap(fieldMap));
      oos.writeFields();
   }
}
