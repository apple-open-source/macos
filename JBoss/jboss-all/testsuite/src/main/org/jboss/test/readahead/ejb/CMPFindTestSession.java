package org.jboss.test.readahead.ejb;

import java.util.Iterator;
import java.util.Collection;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import org.jboss.test.readahead.interfaces.AddressHome;
import org.jboss.test.readahead.interfaces.AddressRemote;
import org.jboss.test.readahead.interfaces.CMPFindTestEntityHome;
import org.jboss.test.readahead.interfaces.CMPFindTestEntityRemote;

/**
 * Implementation class for session bean used in read-ahead finder
 * tests
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: CMPFindTestSession.java,v 1.4 2002/04/17 09:49:50 starksm Exp $
 * 
 * Revision:
 */
public class CMPFindTestSession implements SessionBean {
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
   private static final int DATASET_SIZE = 100;
   
   private SessionContext sessionContext;
   
   public void ejbCreate() {
   }
   public void ejbRemove() {
   }
   public void ejbActivate() {
   }
   public void ejbPassivate() {
   }
   public void setSessionContext(SessionContext context) {
      sessionContext = context;
   }
   
   public void removeTestData() {
      try {
         InitialContext ctx = new InitialContext();
         CMPFindTestEntityHome home = (CMPFindTestEntityHome)ctx.lookup("CMPFindTestEntity");
         
         Collection coll = home.findAll();
         Iterator iter = coll.iterator();
         while (iter.hasNext()) {
            CMPFindTestEntityRemote rem = (CMPFindTestEntityRemote)iter.next();
            
            rem.remove();
         }
      } catch (Exception e) {
      }
   }
   
   public void createTestData() {
      try {
         InitialContext ctx = new InitialContext();
         CMPFindTestEntityHome home = (CMPFindTestEntityHome)ctx.lookup("CMPFindTestEntity");
         AddressHome addrHome = (AddressHome)ctx.lookup("Address");
         for (int i=0;i<DATASET_SIZE;i++) {
            String key = Long.toString(System.currentTimeMillis())+"-"+i;
            CMPFindTestEntityRemote rem = home.create(key);
            rem.setName("Name");
            rem.setRank("Rank");
            rem.setSerialNumber("123456789");
            //give him an address
            if ((i % 2) ==0) {
               addrHome.create(rem.getKey(), "1", "123 east st.", "Eau Claire", "WI", "54701");
            } else {
               addrHome.create(rem.getKey(), "1", "123 east st.", "Milwaukee", "WI", "54201");
            }
         }
      } catch (Exception e) {
         log.debug("Exception caught: "+e);
         log.debug("failed", e);
         throw new EJBException(e.getMessage());
      }
   }
   
   public void addressByCity() {
      try {
         InitialContext ctx = new InitialContext();
         AddressHome home = (AddressHome)ctx.lookup("Address");
         
         long startTime = System.currentTimeMillis();
         Collection coll = home.findByCity("Eau Claire");
         int count = 0;
         Iterator iter = coll.iterator();
         while (iter.hasNext()) {
            AddressRemote rem = (AddressRemote)iter.next();
            rem.getCity();
//            log.debug("Name: "+rem.getName()+" Rank: "+rem.getRank());
            count++;
         }
         long endTime = System.currentTimeMillis();
         log.debug("called "+count+" beans in "+(endTime-startTime)+" ms.");
      } catch (Exception e) {
         log.debug("Caught "+e);
      }
   }
   
   public void testByCity() {
      try {
         InitialContext ctx = new InitialContext();
         CMPFindTestEntityHome home = (CMPFindTestEntityHome)ctx.lookup("CMPFindTestEntity");
         
         long startTime = System.currentTimeMillis();
         Collection coll = home.findByCity("Eau Claire");
         int count = 0;
         Iterator iter = coll.iterator();
         while (iter.hasNext()) {
            CMPFindTestEntityRemote rem = (CMPFindTestEntityRemote)iter.next();
            rem.getName();
//            log.debug("Name: "+rem.getName()+" Rank: "+rem.getRank());
            count++;
         }
         long endTime = System.currentTimeMillis();
         log.debug("called "+count+" beans in "+(endTime-startTime)+" ms.");
      } catch (Exception e) {
      }
   }
   public void testFinder() {
      try {
         InitialContext ctx = new InitialContext();
         CMPFindTestEntityHome home = (CMPFindTestEntityHome)ctx.lookup("CMPFindTestEntity");
         
         long startTime = System.currentTimeMillis();
         Collection coll = home.findAll();
         int count = 0;
         Iterator iter = coll.iterator();
         while (iter.hasNext()) {
            CMPFindTestEntityRemote rem = (CMPFindTestEntityRemote)iter.next();
            rem.getName();
//            log.debug("Name: "+rem.getName()+" Rank: "+rem.getRank());
            count++;
         }
         long endTime = System.currentTimeMillis();
         log.debug("called "+count+" beans in "+(endTime-startTime)+" ms.");
      } catch (Exception e) {
      }
   }
   
   public void testUpdates() {
      try {
         InitialContext ctx = new InitialContext();
         CMPFindTestEntityHome home = (CMPFindTestEntityHome)ctx.lookup("CMPFindTestEntity");
         
         long startTime = System.currentTimeMillis();
         Collection coll = home.findAll();
         int count = 0;
         Iterator iter = coll.iterator();
         while (iter.hasNext()) {
            CMPFindTestEntityRemote rem = (CMPFindTestEntityRemote)iter.next();
            rem.getName();
            rem.setRank("Very");
            count++;
         }
         long endTime = System.currentTimeMillis();
         log.debug("called "+count+" beans in "+(endTime-startTime)+" ms.");
      } catch (Exception e) {
      }
   }
}
