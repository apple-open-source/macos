package org.jboss.test.jmx.attrs;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class AttrTests implements AttrTestsMBean
{
   private String xmlString;
   private String sysPropRef;
   private String trimedString;

   public String getXmlString()
   {
      return xmlString;
   }

   public void setXmlString(String xmlString)
   {
      this.xmlString = xmlString;
   }

   public String getSysPropRef()
   {
      return sysPropRef;
   }

   public void setSysPropRef(String sysPropRef)
   {
      this.sysPropRef = sysPropRef;
   }

   public String getTrimedString()
   {
      return trimedString;
   }

   public void setTrimedString(String trimedString)
   {
      this.trimedString = trimedString;
   }

}
