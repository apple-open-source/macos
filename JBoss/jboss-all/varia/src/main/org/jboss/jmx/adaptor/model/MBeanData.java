package org.jboss.jmx.adaptor.model;

import javax.management.MBeanInfo;
import javax.management.ObjectName;

/** An mbean ObjectName and MBeanInfo pair that is orderable by ObjectName.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.5 $
 */
public class MBeanData implements Comparable
{
   private ObjectName objectName;
   private MBeanInfo metaData;

   /** Creates a new instance of MBeanInfo */
    public MBeanData() {
    }

   /** Creates a new instance of MBeanInfo */
   public MBeanData(ObjectName objectName, MBeanInfo metaData)
   {
      this.objectName = objectName;
      this.metaData = metaData;
   }

   /** Getter for property objectName.
    * @return Value of property objectName.
    */
   public ObjectName getObjectName()
   {
      return objectName;
   }
   
   /** Setter for property objectName.
    * @param objectName New value of property objectName.
    */
   public void setObjectName(ObjectName objectName)
   {
      this.objectName = objectName;
   }

   /** Getter for property metaData.
    * @return Value of property metaData.
    */
   public MBeanInfo getMetaData()
   {
      return metaData;
   }
   
   /** Setter for property metaData.
    * @param metaData New value of property metaData.
    */
   public void setMetaData(MBeanInfo metaData)
   {
      this.metaData = metaData;
   }

   /**
    * @return The ObjectName.toString()
    */
   public String getName()
   {
      return objectName.toString();
   }
   /**
    * @return The canonical key properties string
    */
   public String getNameProperties()
   {
      return objectName.getCanonicalKeyPropertyListString();
   }
   /**
    * @return The MBeanInfo.getClassName() value
    */
   public String getClassName()
   {
      return metaData.getClassName();
   }

   /** Compares MBeanData based on the ObjectName domain name and canonical
    * key properties
    *
    * @param the MBeanData to compare against
    * @return < 0 if this is less than o, > 0 if this is greater than o,
    *    0 if equal.
    */
   public int compareTo(Object o)
   {
      MBeanData md = (MBeanData) o;
      String d1 = objectName.getDomain();
      String d2 = md.objectName.getDomain();
      int compare = d1.compareTo(d2);
      if( compare == 0 )
      {
         String p1 = objectName.getCanonicalKeyPropertyListString();
         String p2 = md.objectName.getCanonicalKeyPropertyListString();
         compare = p1.compareTo(p2);
      }
      return compare;
   }

   public boolean equals(Object o)
   {
      if (o == null || (o instanceof MBeanData) == false)
         return false;
      if (this == o)
         return true;
      return (this.compareTo(o) == 0);
   }
}
