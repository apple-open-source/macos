package org.jboss.test.testbean.bean;


import java.rmi.*;
import javax.ejb.*;
import javax.naming.Context;
import javax.sql.DataSource;
import javax.naming.InitialContext;
import java.sql.Connection;
import org.jboss.test.testbean.interfaces.StatelessSession;
import org.jboss.test.testbean.interfaces.StatelessSessionHome;
import java.util.Collection;
import java.util.Collections;
import java.util.Vector;
import java.util.Enumeration;

public class EntityBMPBean implements EntityBean {
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
  private EntityContext entityContext;
  public String name;
  private StatelessSession statelessSession;
  private boolean wasFind = false;


  public String ejbCreate() throws RemoteException, CreateException {
	  
	   log.debug("EntityBMP.ejbCreate() called");
       this.name = "noname";

       return name;
  
	  
  }
  public String ejbCreate(String name) throws RemoteException, CreateException {

       log.debug("EntityBMP.ejbCreate("+name+") called");
       this.name = name;

       return name;
  }

  public String ejbCreateMETHOD(String name) throws RemoteException, CreateException {

       log.debug("EntityBMP.ejbCreateMETHOD("+name+") called");
       this.name = name;

       return name;
  }

  // For usage in BMP only (The return is the primary key type)

  public String ejbFindByPrimaryKey(String name) throws RemoteException, FinderException {

      log.debug("EntityBMP.ejbFindByPrimaryKey() called");

      wasFind = true;

      return name;
  }

  public Collection ejbFindCollectionKeys(int num) throws RemoteException, FinderException {

      log.debug("EntityBMP.ejbFindMultipleKeys() called with num "+num);
      Collection pks = new Vector();
      pks.add("primary key number 1");
      pks.add("primary key number 2");
      pks.add("primary key number 3");

      return pks;
  }
  public Enumeration ejbFindEnumeratedKeys(int num) throws RemoteException, FinderException {

      log.debug("EntityBMP.ejbFindEnumeratedKeys() called with num "+num);
      Collection pks = new Vector();
      pks.add("primary key number 1");
      pks.add("primary key number 2");
      pks.add("primary key number 3");

      return Collections.enumeration(pks);
  }

  public void ejbPostCreate() throws RemoteException, CreateException {

       log.debug("EntityBMP.ejbPostCreate() called");
  }
  
  public void ejbPostCreate(String name) throws RemoteException, CreateException {

       log.debug("EntityBMP.ejbPostCreate("+name+") called");
  }
  
  public void ejbPostCreateMETHOD(String name) throws RemoteException, CreateException {

       log.debug("EntityBMP.ejbPostCreateMETHOD("+name+") called");
  }

  public void ejbActivate() throws RemoteException {
    log.debug("EntityBMP.ejbActivate() called");
  }

  public void ejbLoad() throws RemoteException {
   log.debug("EntityBMP.ejbLoad() called");
  }

  public void ejbPassivate() throws RemoteException {

     log.debug("Was Find "+wasFind);
     log.debug("EntityBMP.ejbPassivate() called");
  }

  public void ejbRemove() throws RemoteException, RemoveException {
   log.debug("EntityBMP.ejbRemove() called");
  }

  public void ejbStore() throws RemoteException {
	 
   log.debug("EntityBMP.ejbStore() called");
  }

  public String callBusinessMethodA() throws RemoteException{

     log.debug("EntityBMP.callBusinessMethodA() called");
     return "EntityBMP.callBusinessMethodA() called, my primaryKey is "+
            entityContext.getPrimaryKey().toString();
  }

   public String callBusinessMethodB() throws RemoteException {

     log.debug("EntityBMP.callBusinessMethodB() called, calling B2B");
     String b2bMessage = statelessSession.callBusinessMethodB();
     log.debug("EntityBMP called stateless Bean and it said "+b2bMessage);
     return "EntityBMP.callBusinessMethodB() called, called other bean (B2B) and it said \""+b2bMessage+"\"";

  }
  
   public String callBusinessMethodB(String words) throws RemoteException {
       log.debug("EntityBMP.callBusinessMethodB(String) called, calling B2B");
     String b2bMessage = statelessSession.callBusinessMethodB();
     log.debug("EntityBMP called stateless Bean and it said "+b2bMessage);
     return "EntityBMP.callBusinessMethodB() called, called other bean (B2B) and it said \""+b2bMessage+"\""+" words "+words;

  }

  public void setEntityContext(EntityContext context) throws RemoteException {
     log.debug("EntityBMP.setSessionContext() called");
     entityContext = context;

     //we use the setEntityContext to lookup the connection and the EJBReference

        try {

            Context namingContext = new InitialContext();

            Connection connection = ((DataSource) namingContext.lookup("java:comp/env/jdbc/myDatabase")).getConnection();
			connection.close();

            log.debug("EntityBMP I did get the connection to the database for BMP");
        }

        catch (Exception e) {

            log.debug("failed", e);

            log.debug("EntityBMP Could not find the database connection, check settings");

            throw new RemoteException("EntityBMP Could not find the database connection");
        }

        try {

            Context namingContext = new InitialContext();

            log.debug("EntityBMP Looking up Stateless Home");

            StatelessSessionHome statelessHome = ((StatelessSessionHome) namingContext.lookup("java:comp/env/ejb/myEJBRef"));

            log.debug("EntityBMP Calling create on Stateless Home");
            
            statelessSession = statelessHome.create();

            log.debug("EntityBMP Found the EJB Reference in my java:comp/env/ environment");


        }

        catch (Exception e) {

            log.debug("failed", e);

            log.debug("EntityBMP Could not find the EJB Reference called myEJBRef, check settings");

            throw new RemoteException("EntityBean Could not find EJB Reference");
        }
    }

    public void unsetEntityContext() throws RemoteException {

        log.debug("EntityBMP.unsetSessionContext() called");
        entityContext = null;
    }
}
