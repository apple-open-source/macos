package org.jboss.test.jmx.attrs;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface AttrTestsMBean
{
   public String getXmlString();
   public void setXmlString(String xml);

   public String getSysPropRef();
   public void setSysPropRef(String propRef);

   public String getTrimedString();
   public void setTrimedString(String str);

}
