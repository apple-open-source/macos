// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: CMPStateBean.java,v 1.2.2.4 2003/07/26 11:49:42 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session.ejb;

import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import org.jboss.logging.Logger;
import org.mortbay.j2ee.session.interfaces.CMPState;
import org.mortbay.j2ee.session.interfaces.CMPStateHome;
import org.mortbay.j2ee.session.interfaces.CMPStatePK;

/**
 * The Entity bean represents an HttpSession.
 *
 * @author jules@mortbay.com
 * @version $Revision: 1.2.2.4 $
 *
 *   @ejb:bean name="CMPState" type="CMP" view-type="remote" jndi-name="jetty/CMPState" reentrant="true" cmp-version="2.x"
 *   @ejb:interface remote-class="org.mortbay.j2ee.session.interfaces.CMPState" extends="javax.ejb.EJBObject, org.mortbay.j2ee.session.State"
 *   @ejb:pk
 *   @ejb:finder
 *        signature="java.util.Collection findTimedOut(long currentTime, int extraTime, int actualMaxInactiveInterval)"
 *        query="SELECT OBJECT(o) FROM CMPState o WHERE (o.maxInactiveInterval>0 AND o.creationTime < (?1-(1000*(o.maxInactiveInterval+?2)))) OR (o.maxInactiveInterval<1 AND o.creationTime < (?1-(1000*(?3+?2))))"
 *
 *   @jboss:table-name "JETTY_HTTPSESSION_CMPState"
 *   @jboss:create-table create="true"
 *   @jboss:remove-table remove="false"
 *   @jboss:container-configuration name="Sharing Standard CMP 2.x EntityBean"
 *
 */

public abstract class CMPStateBean
  implements EntityBean
{
  protected static final Logger _log=Logger.getLogger(CMPStateBean.class);

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
      CMPStateHome home = (CMPStateHome)_entityContext.getEJBObject().getEJBHome();
      Collection c=(Collection)home.findTimedOut(System.currentTimeMillis(), extraTime, actualMaxInactiveInterval);
      if (_log.isDebugEnabled()) _log.debug("distributed scavenging: "+c);

      // this is not working - what is the class of the Objects returned in the Collection ?
      for (Iterator i=c.iterator(); i.hasNext();)
      {
	//	home.remove((CMPState)i.next()); // doesn't work - WHY?
	((CMPState)i.next()).remove();
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
  // Lifecycle
  //----------------------------------------

  /**
   * Create httpSession.
   *
   * @ejb:create-method
   */
  public CMPStatePK ejbCreate(String context, String id, int maxInactiveInterval, int actualMaxInactiveInterval)
    throws CreateException
  {
    if (_log.isDebugEnabled()) _log.debug("ejbCreate("+context+":"+id+")");

    setContext(context);
    setId(id);
    setMaxInactiveInterval(maxInactiveInterval);
    setActualMaxInactiveInterval(actualMaxInactiveInterval);

    long time=System.currentTimeMillis();
    setCreationTime(time);
    setLastAccessedTime(time);
    setAttributes(new HashMap());

    return null;
  }

  /**
   * Create httpSession.
   *
   */
  public void ejbPostCreate(String context, String id, int maxInactiveInterval, int actualMaxInactiveInterval)
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
   */
  public abstract String getContext();

  /**
   * @ejb:pk-field
   * @ejb:persistent-field
   */
  public abstract void setContext(String context);

  //----------------------------------------
  // Id

  /**
   * @ejb:interface-method
   * @ejb:pk-field
   * @ejb:persistent-field
   */
  public abstract String getId();

  /**
   * @ejb:pk-field
   * @ejb:persistent-field
   */
  public abstract void setId(String id);
  //----------------------------------------
  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract long getCreationTime();

  /**
   * @ejb:persistent-field
   */
  public abstract void setCreationTime(long time);

  //----------------------------------------
  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract long getLastAccessedTime();

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract void setLastAccessedTime(long time);

  //----------------------------------------
  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract int getMaxInactiveInterval();

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract void setMaxInactiveInterval(int interval);

  //----------------------------------------
  // Attributes

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract Map getAttributes();

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract void setAttributes(Map attributes);

  /**
   * @ejb:interface-method
   */
  public Object
    getAttribute(String name)
    //    throws IllegalStateException
  {
    //    _log.info(getId()+": get attribute - "+name);
    return getAttributes().get(name);
  }

  /**
   * @ejb:interface-method
   */
  public Object
    setAttribute(String name, Object value, boolean returnValue)
    //    throws IllegalStateException
  {
    Map attrs=getAttributes();
    Object tmp=attrs.put(name, value);
    setAttributes(null);
    setAttributes(attrs);

    //    _log.info(getContext()+":"+getId()+": set attribute - "+name+":"+value);

    return returnValue?tmp:null;
  }

  /**
   * @ejb:interface-method
   */
  public Object
    removeAttribute(String name, boolean returnValue)
    //    throws IllegalStateException
  {
    Map attrs=getAttributes();
    Object tmp=attrs.remove(name);

    if (tmp!=null)
    {
      setAttributes(null);	// belt-n-braces - TODO
      setAttributes(attrs);
    }

    //    _log.info(getContext()+":"+getId()+": remove attribute - "+name);

    return returnValue?tmp:null;
  }

  /**
   * @ejb:interface-method
   */
  public Enumeration
    getAttributeNameEnumeration()
    //    throws IllegalStateException
  {
    return Collections.enumeration(getAttributes().keySet());
  }

  /**
   * @ejb:interface-method
   */
  public String[]
    getAttributeNameStringArray()
    //    throws IllegalStateException
  {
    Map attrs=getAttributes();
    return (String[])attrs.keySet().toArray(new String[attrs.size()]);
  }

  //----------------------------------------
  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract int getActualMaxInactiveInterval();

  /**
   * @ejb:persistent-field
   */
  public abstract void setActualMaxInactiveInterval(int interval);

  //----------------------------------------
  // new stuff - for server sider validity checking...

  public boolean
    isValid()
  {
    long maxInactiveInterval=getMaxInactiveInterval();
    maxInactiveInterval=(maxInactiveInterval<1?getActualMaxInactiveInterval():maxInactiveInterval)*1000;
    return (getLastAccessedTime()+maxInactiveInterval)>System.currentTimeMillis();
  }
}
