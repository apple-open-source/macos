package org.jboss.jmx.adaptor.model;

import java.util.Arrays;
import java.util.TreeSet;

/** The MBeanData for a given JMX domain name
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.3.2.2 $
 */
public class DomainData
{
   String domainName;
   TreeSet domainData = new TreeSet();

   /** Creates a new instance of MBeanInfo */
   public DomainData(String domainName)
   {
      this.domainName = domainName;
   }
   public DomainData(String domainName, MBeanData[] data)
   {
      this.domainName = domainName;
      domainData.addAll(Arrays.asList(data));
   }

   public int hashCode()
   {
      return domainName.hashCode();
   }
   public boolean equals(Object obj)
   {
      DomainData data = (DomainData) obj;
      return domainName.equals(data.domainName);
   }

   public String getDomainName()
   {
      return domainName;
   }
   public MBeanData[] getData()
   {
      MBeanData[] data = new MBeanData[domainData.size()];
      domainData.toArray(data);
      return data;
   }
   public void addData(MBeanData data)
   {
      domainData.add(data);
   }
}
