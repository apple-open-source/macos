package org.jboss.test.readahead.ejb;

import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.ejb.EntityContext;

/**
 * Implementation class for one of the entities used in read-ahead finder
 * tests
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: CMPFindTestEntity.java,v 1.2 2002/02/15 06:15:55 user57 Exp $
 * 
 * Revision:
 */
public class CMPFindTestEntity implements EntityBean {
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
   EntityContext entityContext;
   public String key;
   public String name;
   public String rank;
   public String serialNumber;
   private boolean modified;
   
   public String ejbCreate(String key) throws CreateException {
      this.key = key;
      return key;
   }
   public boolean isModified() {
      return modified;
   }
   public void ejbPostCreate(String key) throws CreateException {
   }
   public void ejbRemove() throws RemoveException {
   }
   public void ejbActivate() {
   }
   public void ejbPassivate() {
   }
   public void ejbLoad() {
      modified = false;
   }
   public void ejbStore() {
      modified = false;
   }
   public void setEntityContext(EntityContext entityContext) {
      this.entityContext = entityContext;
   }
   public void unsetEntityContext() {
      entityContext = null;
   }
   public String getKey() {
      return key;
   }
   public void setName(String newName) {
      name = newName;
   }
   public String getName() {
      return name;
   }
   public void setRank(String newRank) {
      rank = newRank;
      modified = true;
   }
   public String getRank() {
      return rank;
   }
   public void setSerialNumber(String newSerialNumber) {
      serialNumber = newSerialNumber;
      modified = true;
   }
   public String getSerialNumber() {
      return serialNumber;
   }
}
