/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.varia.autonumber;

import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.naming.InitialContext;

/**
 * ???
 * 
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @version $Revision: 1.1.4.1 $
 */
public class AutoNumberEJB
   implements EntityBean
{
   public String name;
   public Integer value; 
   
   public String ejbCreate(String name) throws CreateException {
      this.name = name;
      this.value = new Integer(0);
      
      return null;
   }
   
   public void ejbPostCreate(String name) {}
   
   public Integer getValue()  {
      return value;
   }
   
   public void setValue(Integer value)  {
      this.value = value;
   }
   
   public void ejbActivate() {}
   public void ejbPassivate() {}
   public void ejbLoad() {}
   public void ejbStore() {}
   public void ejbRemove() {}
   public void setEntityContext(javax.ejb.EntityContext ec) {}
   public void unsetEntityContext() {}
}
