
package org.jboss.test.invokers.ejb;

import java.rmi.RemoteException;
import java.util.Vector;
import java.util.Collection;

import java.sql.*;

import javax.naming.*;
import javax.ejb.*;

public class SimpleBMPBean
implements EntityBean
{
   EntityContext ctx = null;

   // bmp fields
   Integer id;
   String name;
   
   public Integer ejbCreate (int _id, String _name)
      throws CreateException, RemoteException
   {
      id = new Integer (_id);
      name = _name;
      return id;      
   }
   
   public void ejbPostCreate (int _id, String _name)
      throws CreateException, RemoteException
   {
   }
   
   public void ejbLoad ()
   {
   }
   
   public void ejbStore ()
   {
   }
   
   public void ejbRemove ()
   {
   }

   
   public Integer ejbFindByPrimaryKey (Integer _key) throws FinderException
   {
      return _key;
   }

   public void ejbActivate ()
   {
   }
   
   public void ejbPassivate ()
   {
   }
   
   public void setEntityContext (EntityContext _ctx)
   {
      ctx = _ctx;
   }
   
   public void unsetEntityContext ()
   {
      ctx = null;
   }
   
   // business methods ---------------------------------------------------------------
   
   public void setName (String _name)
   {
      name = _name;
   }
   
   public String getName ()
   {
      return name;
   }
   
   
}
