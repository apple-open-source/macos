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

  public CMRBugManagerBean() {}

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
      if (!cmrBugs.isEmpty())
      {
        Iterator i = cmrBugs.entrySet().iterator();
        Map.Entry entry = (Map.Entry) i.next();

        // the root id (of which all others are based) is the first key in
        // the SortedMap
        //
        String root = (String) entry.getKey();

        String id = root;
        String description = (String) entry.getValue();

        CMRBugEJBLocal parent = cmrBugHome.create(id, description, null);
        /*
        if (succeed)
        {
          // replace the description with the id
          //
          entry.setValue(parent.getId());
        }
        else // fail
        {*/
          // replace the description in the map with the actual CMRBugEJBLocal
          //
          entry.setValue(parent);
          //}

        while (i.hasNext())
        {
          entry = (Map.Entry) i.next();

          id = (String) entry.getKey();
          description = (String) entry.getValue();

          int index = id.lastIndexOf(".");
          if (index != -1)
          {
            // determine the parent id and then try to find the parent's
            // CMRBugEJBLocal in the map
            //
            String parentId = id.substring(0, index);
            /*
            if (succeed)
            {
              parent =
                cmrBugHome.findByPrimaryKey((String) cmrBugs.get(parentId));
            }
            else // fail (sometimes)
            {*/
              parent = (CMRBugEJBLocal) cmrBugs.get(parentId);
              //}
          }
          /*
          if (succeed)
          {
            // replace the description with the id
            //
            entry.setValue(cmrBugHome.create(id, description, parent).getId());
          }
          else // fail (sometimes)
          {*/
            // replace the description in the map with the actual CMRBugEJBLocal
            //
            entry.setValue(cmrBugHome.create(id, description, parent));
            //}
        }
      }
    }
    catch (Exception e)
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
      if (parent != null)
      {
        parentIdAndDescription = new String[2];
        parentIdAndDescription[0] = parent.getId();
        parentIdAndDescription[1] = parent.getDescription();
      }

      return parentIdAndDescription;
    }
    catch (Exception e)
    {
      e.printStackTrace();
      throw new EJBException(e.getMessage());
    }
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
    catch (Exception e)
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
    catch (Exception e)
    {
      throw new EJBException(e.getMessage());
    }
  }

  public void ejbPassivate()
  {
    cmrBugHome = null;
  }

  public void ejbRemove() {}

  public void setSessionContext(SessionContext sessionContext) {}

  private CMRBugEJBLocalHome lookupCMRBugHome()
    throws NamingException
  {
    InitialContext initialContext = new InitialContext();
    return (CMRBugEJBLocalHome) initialContext.lookup("java:comp/env/ejb/CMRBug");
  }
}
