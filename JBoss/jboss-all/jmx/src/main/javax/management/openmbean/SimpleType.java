/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.io.InvalidClassException;
import java.io.ObjectStreamException;
import java.math.BigDecimal;
import java.math.BigInteger;
import javax.management.ObjectName;

/**
 * The open type for simple java classes. These are a fixed number of these.
 *
 * The open types are available as static constants from this class.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public final class SimpleType
   extends OpenType
{
   // Attributes ----------------------------------------------------

   /**
    * Cached hash code
    */
   private transient int cachedHashCode = 0;

   /**
    * Cached string representation
    */
   private transient String cachedToString = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID = 2215577471957694503L;

   /**
    * The simple type for java.math.BigDecimal
    */
   public static final SimpleType BIGDECIMAL;

   /**
    * The simple type for java.math.BigInteger
    */
   public static final SimpleType BIGINTEGER;

   /**
    * The simple type for java.lang.Boolean
    */
   public static final SimpleType BOOLEAN;

   /**
    * The simple type for java.lang.Byte
    */
   public static final SimpleType BYTE;

   /**
    * The simple type for java.lang.Character
    */
   public static final SimpleType CHARACTER;

   /**
    * The simple type for java.lang.Double
    */
   public static final SimpleType DOUBLE;

   /**
    * The simple type for java.lang.Float
    */
   public static final SimpleType FLOAT;

   /**
    * The simple type for java.lang.Integer
    */
   public static final SimpleType INTEGER;

   /**
    * The simple type for java.lang.Long
    */
   public static final SimpleType LONG;

   /**
    * The simple type for javax.management.ObjectName
    */
   public static final SimpleType OBJECTNAME;

   /**
    * The simple type for java.lang.Short
    */
   public static final SimpleType SHORT;

   /**
    * The simple type for java.lang.String
    */
   public static final SimpleType STRING;

   /**
    * The simple type for java.lang.Void
    */
   public static final SimpleType VOID;

   static
   {
      try
      {
         BIGDECIMAL = new SimpleType(BigDecimal.class.getName());
         BIGINTEGER = new SimpleType(BigInteger.class.getName());
         BOOLEAN = new SimpleType(Boolean.class.getName());
         BYTE = new SimpleType(Byte.class.getName());
         CHARACTER = new SimpleType(Character.class.getName());
         DOUBLE = new SimpleType(Double.class.getName());
         FLOAT = new SimpleType(Float.class.getName());
         INTEGER = new SimpleType(Integer.class.getName());
         LONG = new SimpleType(Long.class.getName());
         OBJECTNAME = new SimpleType(ObjectName.class.getName());
         SHORT = new SimpleType(Short.class.getName());
         STRING = new SimpleType(String.class.getName());
         VOID = new SimpleType(Void.class.getName());
      }
      catch (OpenDataException e)
      {
         throw new RuntimeException(e.toString());
      }
   }

   // Constructors --------------------------------------------------

   /**
    * Construct an SimpleType.<p>
    *
    * This constructor is used to construct the static simple types.
    *
    * @param className the name of the class implementing the open type
    */
   private SimpleType(String className)
      throws OpenDataException
   {
      super(className, className, className);
   }

   // OpenType Overrides---------------------------------------------

   public boolean isValue(Object obj)
   {
       return (obj != null && obj.getClass().getName().equals(getClassName()));
   }

   // Serializable Implementation -----------------------------------

   public Object readResolve()
      throws ObjectStreamException
   {
      String className = getClassName();
      if (className.equals(STRING.getClassName()))
         return STRING;
      if (className.equals(INTEGER.getClassName()))
         return INTEGER;
      if (className.equals(BOOLEAN.getClassName()))
         return BOOLEAN;
      if (className.equals(OBJECTNAME.getClassName()))
         return OBJECTNAME;
      if (className.equals(LONG.getClassName()))
         return LONG;
      if (className.equals(BYTE.getClassName()))
         return BYTE;
      if (className.equals(CHARACTER.getClassName()))
         return CHARACTER;
      if (className.equals(DOUBLE.getClassName()))
         return DOUBLE;
      if (className.equals(FLOAT.getClassName()))
         return FLOAT;
      if (className.equals(SHORT.getClassName()))
         return SHORT;
      if (className.equals(BIGDECIMAL.getClassName()))
         return BIGDECIMAL;
      if (className.equals(BIGINTEGER.getClassName()))
         return BIGINTEGER;
      if (className.equals(VOID.getClassName()))
         return VOID;
      throw new InvalidClassException(className);
   }

   // Object Overrides ----------------------------------------------

   public boolean equals(Object obj)
   {
      if (this == obj)
         return true;
      if (obj == null || !(obj instanceof SimpleType))
         return false;
      return (this.getClassName().equals(((SimpleType) obj).getClassName()));
   }

   public int hashCode()
   {
      if (cachedHashCode != 0)
         return cachedHashCode;
      cachedHashCode = getClassName().hashCode();
      return cachedHashCode;
   }

   public String toString()
   {
      if (cachedToString != null)
         return cachedToString;
      StringBuffer buffer = new StringBuffer(SimpleType.class.getName());
      buffer.append(":");
      buffer.append(getClassName());
      cachedToString = buffer.toString();
      return cachedToString;
   }

   // Private -------------------------------------------------------
}
