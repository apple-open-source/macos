/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * This interface defines behavioral and runtime metadata for ModelMBeans.
 * A descriptor is a set of name-value pairs.<p>
 *
 * The {@link DescriptorAccess} interface defines how to get and set
 * Descriptors for a ModelMBean's metadata classes.<p>
 *
 * The implementation must implement the following constructors.<p>
 *
 * <i>Descriptor()</i> returns an empty descriptor.<p>
 *
 * <i>Descriptor(Descriptor)</i> returns a copy of the decriptor.<p>
 *
 * <i>Descriptor(String[], Object[])</i> a constructor that verifies the
 * field names include a descriptorType and that predefined fields
 * contain valid values.<p>
 *
 * @see javax.management.DescriptorAccess
 * @see javax.management.modelmbean.ModelMBean
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 *
 */
public interface Descriptor
   extends Serializable
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * Retrieves the value of a field.
    *
    * @param fieldName the name of the field.
    * @return the value of the field.
    * @exception RuntimeOperationsException when the field name is not
    * valid.
    */
   public Object getFieldValue(String fieldName)
      throws RuntimeOperationsException;

   /**
    * Sets the value of a field.
    *
    * @param fieldName the name of the field.
    * @param fieldValue the value of the field.
    * @exception RuntimeOperationsException when the field name or value
    * is not valid.
    */
   public void setField(String fieldName, Object fieldValue)
      throws RuntimeOperationsException;

   /**
    * Retrieves all the fields in this descriptor. The format is
    * <i>fieldName=fieldValue</i> for String values and
    * <i>fieldName=(fieldValue.toString())</i> for other value types.<p>
    *
    * @return an array of fieldName=fieldValue strings.
    */
   public String[] getFields();

   /**
    * Retrieves all the field names in this descriptor.
    *
    * @return an array of field name strings.
    */
   public String[] getFieldNames();

   /**
    * Retrieves all the field values for the passed field names. The
    * array of object values returned is in the same order as the
    * field names passed to the method.<p>
    *
    * When a fieldName does not exist, the corresponding element of
    * the returned array is null.<p>
    *
    * @param fieldNames the array of field names to retrieve. Pass null
    * to retrieve all fields.
    * @return an array of field values.
    */
   public Object[] getFieldValues(String[] fieldNames);

   /**
    * Remove a field from the descriptor.
    *
    * @param fieldName the field to remove. No exception is thrown
    * when the field is not in the descriptor.
    */
   public void removeField(String fieldName);


   /**
    * Set multiple fields in this descriptor. The field in the fieldNames
    * array is set to the value of the corresponding fieldValues array.
    * The two arrays must be the same size.
    *
    * @param fieldNames an array of fieldNames to set. Neither the array
    * or array elements can be null. The fieldName must exist.
    * @param fieldValues an array of fieldValues to set. Neither the array
    * or array elements can be null. The fieldValue must be valid for
    * the corresponding fieldName.
    * @exception RuntimeOperationsException for not existent or fieldNames,
    * invalid or null fieldValues, the two arrays are different sizes or
    * the contructor fails for any reason.
    */
   public void setFields(String[] fieldNames, Object[] fieldValues)
      throws RuntimeOperationsException;

   /**
    * Returns a descriptor that is a duplicate of this one.
    *
    * @exception RuntimeOperationsException for invalid fieldNames,
    * fieldValues or the contructor fails for any reason.
    */
   public java.lang.Object clone()
      throws RuntimeOperationsException;

   /**
    * Checks to see that this descriptor is valid.
    *
    * @return true when the fieldValues are legal for the fieldNames,
    * false otherwise.
    * @exception RuntimeOperationsException for any error performing
    * the validation.
    */
   public boolean isValid()
         throws RuntimeOperationsException;
}
