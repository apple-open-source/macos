/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.cmr.ejb;


import java.util.Iterator;
import java.util.Map;
import java.util.SortedMap;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.apache.log4j.Category;

import org.jboss.test.cmp2.cmr.interfaces.CMRBugEJBLocalHome;
import org.jboss.test.cmp2.cmr.interfaces.CMRBugEJBLocal;

/**
 * Describe class <code>CMRBugManagerBean</code> here.
 *
 * @author <a href="mailto:MNewcomb@tacintel.com">Michael Newcomb</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version 1.0
 * @ejb:bean type="Stateless" name="CMRBugManagerEJB" jndi-name="CMRBugManager"
 * @ejb:ejb-ref ejb-name="CMRBugEJB"
 *              view-type="local"
 *              ref-name="ejb/CMRBug"
 * @ejb:transaction type="Required"
 * @ejb:transaction-type type="Container"
 */
public class CMRBugManagerBean
   implements SessionBean
{
   private CMRBugEJBLocalHome cmrBugHome;

   private Category log = Category.getInstance(getClass());

   public CMRBugManagerBean()
   {
   }

   /**
    * Describe <code>createCMRBugs</code> method here.
    *
    * @param cmrBugs a <code>SortedMap</code> value
    * @ejb:interface-method view-type="remote"
    */
   public void createCMRBugs(SortedMap cmrBugs)
   {
      try
      {
         if(!cmrBugs.isEmpty())
         {
            Iterator i = cmrBugs.entrySet().iterator();
            Map.Entry entry = (Map.Entry)i.next();

            // the root id (of which all others are based) is the first key in
            // the SortedMap
            //
            String root = (String)entry.getKey();

            String id = root;
            String description = (String)entry.getValue();

            CMRBugEJBLocal parent = cmrBugHome.create(id, description, null);
            entry.setValue(parent);

            while(i.hasNext())
            {
               entry = (Map.Entry)i.next();

               id = (String)entry.getKey();
               description = (String)entry.getValue();

               int index = id.lastIndexOf(".");
               if(index != -1)
               {
                  // determine the parent id and then try to find the parent's
                  // CMRBugEJBLocal in the map
                  //
                  String parentId = id.substring(0, index);
                  parent = (CMRBugEJBLocal)cmrBugs.get(parentId);
               }
               entry.setValue(cmrBugHome.create(id, description, parent));
            }
         }
      }
      catch(Exception e)
      {
         e.printStackTrace();
         throw new EJBException(e.getMessage());
      }
   }

   /**
    * Describe <code>getParentFor</code> method here.
    *
    * @param id a <code>String</code> value
    * @return a <code>String[]</code> value
    * @ejb:interface-method view-type="remote"
    */
   public String[] getParentFor(String id)
   {
      try
      {
         CMRBugEJBLocal cmrBug = cmrBugHome.findByPrimaryKey(id);
         CMRBugEJBLocal parent = cmrBug.getParent();

         String[] parentIdAndDescription = null;
         if(parent != null)
         {
            parentIdAndDescription = new String[2];
            parentIdAndDescription[0] = parent.getId();
            parentIdAndDescription[1] = parent.getDescription();
         }

         return parentIdAndDescription;
      }
      catch(Exception e)
      {
         e.printStackTrace();
         throw new EJBException(e.getMessage());
      }
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void setupLoadFKState()
      throws Exception
   {
      CMRBugEJBLocal bug1 = cmrBugHome.create("first", null, null);
      CMRBugEJBLocal bug2 = cmrBugHome.create("second", null, null);
      CMRBugEJBLocal bug3 = cmrBugHome.create("third", null, null);
      CMRBugEJBLocal bug4 = cmrBugHome.create("forth", null, null);

      bug1.setNextNode(bug2);
      bug2.setNextNode(bug3);
      bug3.setNextNode(bug4);

      bug4.setPrevNode(bug3);
      bug3.setPrevNode(bug2);
      bug2.setPrevNode(bug1);
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void moveLastNodeBack()
      throws Exception
   {
      CMRBugEJBLocal bug = cmrBugHome.findByPrimaryKey("forth");

      CMRBugEJBLocal prev = bug.getPrevNode();
      CMRBugEJBLocal next = bug.getNextNode();
      CMRBugEJBLocal prevPrev = prev.getPrevNode();

      prevPrev.setNextNode(bug);
      bug.setPrevNode(prevPrev);
      bug.setNextNode(prev);
      prev.setPrevNode(bug);
      prev.setNextNode(next);
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public boolean lastHasNextNode()
      throws Exception
   {
      CMRBugEJBLocal bug = cmrBugHome.findByPrimaryKey("third");
      return bug.getNextNode() != null;
   }

   /**
    * @ejb.interface-method
    * @ejb.transaction type="RequiresNew"
    */
   public void tearDownLoadFKState()
      throws Exception
   {
      cmrBugHome.remove("first");
      cmrBugHome.remove("second");
      cmrBugHome.remove("third");
      cmrBugHome.remove("forth");
   }

   // --------------------------------------------------------------------------
   // SessionBean methods
   //

   /**
    * Describe <code>ejbCreate</code> method here.
    *
    * @exception CreateException if an error occurs
    */
   public void ejbCreate()
      throws CreateException
   {
      try
      {
         cmrBugHome = lookupCMRBugHome();
      }
      catch(Exception e)
      {
         throw new CreateException(e.getMessage());
      }
   }

   public void ejbActivate()
   {
      try
      {
         cmrBugHome = lookupCMRBugHome();
      }
      catch(Exception e)
      {
         throw new EJBException(e.getMessage());
      }
   }

   public void ejbPassivate()
   {
      cmrBugHome = null;
   }

   public void ejbRemove()
   {
   }

   public void setSessionContext(SessionContext sessionContext)
   {
   }

   private CMRBugEJBLocalHome lookupCMRBugHome()
      throws NamingException
   {
      InitialContext initialContext = new InitialContext();
      return (CMRBugEJBLocalHome)initialContext.lookup("java:comp/env/ejb/CMRBug");
   }
}
