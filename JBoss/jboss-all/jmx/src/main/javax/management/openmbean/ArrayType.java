/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

/**
 * Array types.<p>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class ArrayType
   extends OpenType
{
   // Attributes ----------------------------------------------------

   /**
    * The number of dimensions in the array
    */
   private int dimension = 0;

   /**
    * The element type for the array
    */
   private OpenType elementType = null;

   /**
    * Cached hash code
    */
   private transient int cachedHashCode = 0;

   /**
    * Cached string representation
    */
   private transient String cachedToString = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID = 720504429830309770L;

   // Constructors --------------------------------------------------

   /**
    * Construct an ArrayType.
    *
    * @param dimension the number of dimensions in the array
    * @param elementType the open type of the array elements
    * @exception OpenDataException when open type is an ArrayType
    * @exception IllegalArgumentException for a null argument or 
    *            non-negative dimension
    */
   public ArrayType(int dimension, OpenType elementType)
      throws OpenDataException
   {
      super(genName(dimension, elementType), 
            genName(dimension, elementType),
            genDesc(dimension, elementType));
      this.dimension = dimension;
      this.elementType = elementType;
   }

   // Public --------------------------------------------------------

   /**
    * Get the dimension of the array
    *
    * @return the dimension
    */
   public int getDimension()
   {
      return dimension;
   }

   /**
    * Get the open type of the array elements
    *
    * @return the element type
    */
   public OpenType getElementOpenType()
   {
      return elementType;
   }

   // OpenType Overrides --------------------------------------------

   public boolean isValue(Object obj)
   {
      if (obj == null)
         return false;
      Class clazz = obj.getClass();
      if (clazz.isArray() == false)
         return false;
      if (elementType instanceof SimpleType)
         return (getClassName().equals(clazz.getName()));
      if (elementType instanceof TabularType || 
          elementType instanceof CompositeType)
      {
         Class thisClass = null;
         try
         {
            thisClass = Thread.currentThread().getContextClassLoader().loadClass(getClassName());
         }
         catch (ClassNotFoundException e)
         {
            return false;
         }
         if (thisClass.isAssignableFrom(clazz) == false)
            return false;
         return recursiveCheck((Object[]) obj, dimension);
      }
      return false;
   }

   // Object Overrides ----------------------------------------------

   public boolean equals(Object obj)
   {
      if (this == obj)
         return true;
      if (obj == null || !(obj instanceof ArrayType))
         return false;
      ArrayType other = (ArrayType) obj;
      return (this.getDimension() == other.getDimension() &&
              this.getElementOpenType().equals(other.getElementOpenType()));
   }

   public int hashCode()
   {
      if (cachedHashCode != 0)
         return cachedHashCode;
      cachedHashCode = getDimension() + getElementOpenType().hashCode();
      return cachedHashCode;
   }

   public String toString()
   {
      if (cachedToString != null)
         return cachedToString;
      StringBuffer buffer = new StringBuffer(ArrayType.class.getName());
      buffer.append("\n");
      buffer.append(getTypeName());
      buffer.append("\n");
      buffer.append(new Integer(dimension));
      buffer.append("-dimensional array of\n");
      buffer.append(elementType);
      cachedToString = buffer.toString();
      return cachedToString;
   }

   // Private -------------------------------------------------------

   /**
    * Generate the class and type name<p>
    *
    * NOTE: The spec's javadoc is wrong, it misses the L
    */
   private static String genName(int dimension, OpenType elementType)
      throws OpenDataException
   {
      if (dimension < 1)
         throw new IllegalArgumentException("negative dimension");
      if (elementType == null)
         throw new IllegalArgumentException("null element type");
      if (elementType instanceof ArrayType)
         throw new OpenDataException("array type cannot be an" +
            " element of an array type");
      StringBuffer buffer = new StringBuffer();
      for (int i=0; i < dimension; i++)
         buffer.append('[');
      buffer.append('L');
      buffer.append(elementType.getClassName());
      buffer.append(';');
      return buffer.toString();
   }

   /**
    * Generate the description
    */
   private static String genDesc(int dimension, OpenType elementType)
   {
      StringBuffer buffer = new StringBuffer();
      buffer.append(new Integer(dimension));
      buffer.append("-dimension array of ");
      buffer.append(elementType.getClassName());
      return buffer.toString();
   }

   /**
    * Recursively check array elements
    */
   private boolean recursiveCheck(Object[] elements, int dimension)
   {
      // Reached the end
      if (dimension == 1)
      {
         // Check each element is the correct type
         for (int i = 0; i < elements.length; i++)
            if (elements[i] != null && elementType.isValue(elements[i]) == false)
               return false;
      }
      else
      {
         // Check the array element in this array element
         for (int i = 0; i < elements.length; i++)
            if (recursiveCheck((Object[]) elements[i], dimension-1) == false)
               return false;
      }
      return true;
   }
}
