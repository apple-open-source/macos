// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: CMRAttributeBean.java,v 1.1.2.4 2003/07/26 11:49:42 jules_gosnell Exp $
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
import org.mortbay.j2ee.session.interfaces.CMRAttribute;
import org.mortbay.j2ee.session.interfaces.CMRAttributeHome;
import org.mortbay.j2ee.session.interfaces.CMRAttributePK;

/**webtest.ear/ejb.jar/src/org/mortbay/j2ee/session/interfaces
 * The Entity bean represents an HttpSession.
 *
 * @author jules@mortbay.com
 * @version $Revision: 1.1.2.4 $
 *
 *   @ejb:bean
 *     name="CMRAttribute"
 *     type="CMP"
 *     view-type="both"
 *     jndi-name="CMRAttribute"
 *     local-jndi-name="LocalCMRAttribute"
 *     reentrant="true"
 *     cmp-version="2.x"
 *   @ejb:pk
 *     generate="true"
 *
 *   @jboss:table-name "JETTY_HTTPSESSION_CMRAttribute"
 *   @jboss:create-table create="true"
 *   @jboss:remove-table remove="false"
 *   @jboss:container-configuration name="Standard CMP 2.x EntityBean"
 *
 */

public abstract class CMRAttributeBean
  implements EntityBean
{
  protected static final Logger _log=Logger.getLogger(CMRAttributeBean.class);

  //----------------------------------------
  // Home
  //----------------------------------------

  //----------------------------------------
  // Lifecycle
  //----------------------------------------

  /**
   * Create httpSession Attribute.
   *
   * @ejb:create-method
   */
  public CMRAttributePK
    ejbCreate(String context, String id, String name, Object value)
    throws CreateException
  {
    _log.info("ejbCreate("+context+"-"+id+"-"+name+")");

    setContext(context);
    setId(id);
    setName(name);
    setValue(value);

    return null;
  }

  /**
   * Create httpSession Attribute.
   *
   */
  public void
    ejbPostCreate(String context, String id, String name, Object value)
    throws CreateException
  {
    _log.info("ejbPostCreate("+name+")");
  }

  /**
   */
  public void
    ejbRemove()
    throws RemoveException
  {
    _log.info("ejbRemove("+getContext()+"-"+getId()+"-"+getName()+")");
  }

  //----------------------------------------
  // Accessors
  //----------------------------------------
  // Context

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   * @ejb:pk-field
   */
  public abstract String getContext();
  public abstract void setContext(String context);

  //----------------------------------------
  // Id

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   * @ejb:pk-field
   */
  public abstract String getId();
  public abstract void setId(String id);

  //----------------------------------------
  // Name

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   * @ejb:pk-field
   */
  public abstract String getName();
  public abstract void setName(String name);

  //----------------------------------------
  // Value

  /**
   * @ejb:interface-method
   * @ejb:persistent-field
   */
  public abstract Object getValue();

  /**
   * @ejb:interface-method
   */
  public abstract void setValue(Object value);
}
