// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: CMRStateBean.java,v 1.1.2.4 2003/07/26 11:49:42 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session.ejb;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import org.jboss.logging.Logger;
import org.mortbay.j2ee.session.interfaces.CMRAttributeLocal;
import org.mortbay.j2ee.session.interfaces.CMRAttributeLocalHome;
import org.mortbay.j2ee.session.interfaces.CMRState;
import org.mortbay.j2ee.session.interfaces.CMRStateHome;
import org.mortbay.j2ee.session.interfaces.CMRStatePK;

/**
 * The Entity bean represents an HttpSession.
 *
 * @author jules@mortbay.com
 * @version $Revision: 1.1.2.4 $
 *
 *   @ejb:bean
 *     name="CMRState"
 *     type="CMP"
 *     view-type="remote"
 *     jndi-name="jetty/CMRState"
 *     reentrant="true"
 *     cmp-version="2.x"
 *   @ejb:interface
 *     remote-class="org.mortbay.j2ee.session.interfaces.CMRState"
 *     extends="javax.ejb.EJBObject, org.mortbay.j2ee.session.State"
 *   @ejb:pk
 *     generate="true"
 *   @ejb:ejb-ref
 *     ejb-name="CMRAttribute"
 *     view-type="local"
 *   @ejb:finder
 *        signature="java.util.Collection findTimedOut(long currentTime, int extraTime, int actualMaxInactiveInterval)"
 *        query="SELECT OBJECT(o) FROM CMRState o WHERE (o.maxInactiveInterval>0 AND o.creationTime < (?1-(1000*(o.maxInactiveInterval+?2)))) OR (o.maxInactiveInterval<1 AND o.creationTime < (?1-(1000*(?3+?2))))"
 *
 *   @jboss:table-name "JETTY_HTTPSESSION_CMRState"
 *   @jboss:create-table create="true"
 *   @jboss:remove-table remove="false"
 *   @jboss:container-configuration name="Sharing CMP 2.x EntityBean"
 *
 */

public abstract class CMRStateBean
  implements EntityBean
{
  protected static final Logger _log=Logger.getLogger(CMRStateBean.class);

  //----------------------------------------
  // Home
  //----------------------------------------

  /**
   * This removes sessions that timed out and, for some reason, have
   * not been removed by the webapp that created them (Perhaps it died
   * and has never been redeployed). For this reason, they cannot be
   * loaded back into a VM and invalidated with all the attendant
   * notifications etc (since the classes that they deserialise into
   * are probably not in the caller's ClassLoader and half of the
   * Listeners will have disappeared), so we just remove them directly
   * from the DB.
   *
   * @ejb:home-method
   */
  public void
    ejbHomeScavenge(int extraTime, int actualMaxInactiveInterval)
  {
    try
    {
      // this may not be the best way to call a home method from an
      // instance - but it is the only one that I have found so far...
      CMRStateHome home = (CMRStateHome)_entityContext.getEJBObject().getEJBHome();
      Collection c=(Collection)home.findTimedOut(System.currentTimeMillis(), extraTime, actualMaxInactiveInterval);
      if (_log.isDebugEnabled()) _log.debug("distributed scavenging: "+c);

      // this is not working - what is the class of the Objects returned in the Collection ?
      for (Iterator i=c.iterator(); i.hasNext();)
      {
	//	home.remove((CMRState)i.next()); // doesn't work - WHY?
	((CMRState)i.next()).remove();
      }
      c.clear();
      c=null;
    }
    catch (Exception e)
    {
      _log.warn("could not scavenge dead sessions: ", e);
    }
  }

  //----------------------------------------

  protected CMRAttributeLocalHome _attrHome;

  protected CMRAttributeLocalHome
    lookupCMRAttributeHome()
    throws NamingException
  {
    return (CMRAttributeLocalHome)new InitialContext().lookup("java:comp/env/ejb/CMRAttribute");
  }

  //----------------------------------------
  // Lifecycle
  //----------------------------------------

  /**
   * Create httpSession.
   *
   * @ejb:create-method
   */
  public CMRStatePK ejbCreate(String context, String id, int maxInactiveInterval)
    throws CreateException
  {
    if (_log.isDebugEnabled()) _log.debug("ejbCreate("+context+":"+id+")");

    setContext(context);
    setId(id);
    setMaxInactiveInterval(maxInactiveInterval);

    long time=System.currentTimeMillis();
    setCreationTime(time);
    setLastAccessedTime(time);

    try
    {
      _attrHome=lookupCMRAttributeHome();
    }
    catch (Exception e)
    {
      _log.error("could not find Attribute Home", e);
      throw new CreateException(e.getMessage());
    }

    return null;
  }

  public void
    ejbActivate()
  {
    try
    {
      _attrHome=lookupCMRAttributeHome();
    }
    catch (Exception e)
    {
      throw new EJBException(e.getMessage());
    }
  }

  public void
    ejbPassivate()
  {
    _attrHome = null;
  }

  /**
   * Create httpSession.
   *
   */
  public void ejbPostCreate(String context, String id, int maxInactiveInterval)
    throws CreateException
  {
    //    _log.info("ejbPostCreate("+id+")");
  }

  private EntityContext _entityContext;

  public void
    setEntityContext(EntityContext entityContext)
  {
    //    _log.info("setEntityContext("+ctx+")");
    _entityContext=entityContext;
  }

  /**
   */
  public void
    ejbRemove()
    throws RemoveException
  {
    if (_log.isDebugEnabled()) _log.debug("ejbRemove("+getContext()+":"+getId()+")");
  }

  //----------------------------------------
  // Accessors
  //----------------------------------------
  // Context

  /**
   * @ejb:pk-field
   * @ejb:persistent-field
   *
   */
  public abstract String getContext();

  /**
   * @ejb:pk-field
   * @ejb:persistent-field
   *
   */
  public abstract void setContext(String context);

  //----------------------------------------
  // Id

  /**
   * @ejb:pk-field
   * @ejb:persistent-field
   *
   */
  public abstract String getId();

  /**
   * @ejb:pk-field
   * @ejb:persistent-field
   *
   */
  public abstract void setId(String id);

  //----------------------------------------
  // Attributes
  //----------------------------------------

  /**
   * @ejb:interface-method
   * @ejb:relation
   *     name="CMRState1-CMRAttributeN"
   *     role-name="State-has-Attributes"
   *     cascade-delete="no"
   *     multiple="yes"
   *     target-ejb="CMRAttribute"
   *     target-role-name="Attribute-belongsto-State"
   *     target-cascade-delete="yes"
   *     target-multiple="no"
   *
   * @jboss:relation
   *     fk-constraint="false"
   *     related-pk-field="context"
   *     fk-column="context"
   * @jboss:relation
   *     fk-constraint="false"
   *     related-pk-field="id"
   *     fk-column="id"
   */
  public abstract Set getAttributeBeans();
  /**
   * @ejb:interface-method
   */
  public abstract Set setAttributeBeans(Set attributeBeans);

  //----------------------------------------

  /**
   * @ejb:interface-method
   */
  public Map
    getAttributes()
  {
    Collection attributeBeans=getAttributeBeans();
    Map attrs=new HashMap(attributeBeans.size());

    for (Iterator i=attributeBeans.iterator(); i.hasNext();)
    {
      CMRAttributeLocal attr=(CMRAttributeLocal)i.next();
      attrs.put(attr.getName(), attr.getValue());
    }

    // if there are no attrs do we return empty map or null ?
    // could we put a Map facade around our Collection and return that ?
    return attrs;
  }

  /**
   * @ejb:interface-method
   */
  public void
    setAttributes(Map attributes)
  {
    Collection attributeBeans=getAttributeBeans();
    attributeBeans.clear();
    for (Iterator i=attributes.entrySet().iterator(); i.hasNext();)
    {
      Map.Entry entry=(Map.Entry)i.next();

      try
      {
	attributeBeans.add(_attrHome.create(getContext(), getId(),(String)entry.getKey(),entry.getValue()));
      }
      catch (Exception e)
      {
	_log.error("could not add attribute for - "+entry.getKey()+":"+entry.getValue());
      }
    }
  }

  // auxilliary method - there has to be a faster way than a linear
  // search through a collection...
  protected CMRAttributeLocal
    findAttributeBean(Collection attributeBeans, String name)
  {
    //      CMRAttributeLocal attr=_attrHome.findByPrimaryKey(name);

    for (Iterator i=attributeBeans.iterator(); i.hasNext();)
    {
      CMRAttributeLocal attr=(CMRAttributeLocal)i.next();
      if (attr.getName().equals(name))
	return attr;
    }
    return null;
  }

  /**
   * @ejb:interface-method
   */
  public Object
    getAttribute(String name)
  {
    Collection attributeBeans=getAttributeBeans();
    CMRAttributeLocal attr=findAttributeBean(attributeBeans, name);

    return (attr==null)?null:attr.getValue();
  }

  /**
   * @ejb:interface-method
   */
  public Object
    setAttribute(String name, Object value, boolean returnValue)
  {
    Collection attributeBeans=getAttributeBeans();
    CMRAttributeLocal attr=findAttributeBean(attributeBeans, name);

    if (attr!=null)
    {
      // one already exists...
      Object tmp=attr.getValue();
      attr.setValue(value);
      return returnValue?tmp:null;
    }
    else
    {
      // add a new one...
      try
      {
	attributeBeans.add(_attrHome.create(getContext(), getId(), name, value));
      }
      catch (CreateException e)
      {
	_log.error("could not add attribute for - "+name+":"+value);
	throw new EJBException(e.getMessage());
      }
      return null;
    }
  }

  /**
   * @ejb:interface-method
   */
  public Object
    removeAttribute(String name, boolean returnValue)
  {
    Collection attributeBeans=getAttributeBeans();
    CMRAttributeLocal attr=findAttributeBean(attributeBeans, name);

    if (attr!=null)
    {
      Object tmp=attr.getValue();
      attributeBeans.remove(attr);
      return returnValue?tmp:null;
    }
    else
      return null;
  }

  /**
   * @ejb:interface-method
   */
  public Enumeration
    getAttributeNameEnumeration()
  {
    // is there a better way ?
    Collection attrs=getAttributeBeans();
    ArrayList names=new ArrayList(attrs.size());
    for (Iterator i=attrs.iterator(); i.hasNext();)
      names.add(((CMRAttributeLocal)i.next()).getName());

    return Collections.enumeration(names);
  }

  /**
   * @ejb:interface-method
   */
  public String[]
    getAttributeNameStringArray()
  {
    Collection attrs=getAttributeBeans();
    String[] names=new String[attrs.size()];
    Iterator i=attrs.iterator();
    int j=0;
    while (i.hasNext())
      names[j++]=((CMRAttributeLocal)i.next()).getName();

    return names;
  }

  //----------------------------------------

  /**
   * @ejb:persistent-field
   */
  public abstract void setCreationTime(long time);

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract long getCreationTime();

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract void setLastAccessedTime(long time);

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract long getLastAccessedTime();

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract void setMaxInactiveInterval(int interval);

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract int getMaxInactiveInterval();
}
