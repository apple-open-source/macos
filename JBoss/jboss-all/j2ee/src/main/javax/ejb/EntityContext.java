/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * <P>The EntityContext interface provides an instance with access to
 * the container-provided runtime context of an entity enterprise Bean
 * instance. The container passes the EntityContext interface to an entity
 * enterprise Bean instance after the instance has been created.</P>
 * 
 * <P>The EntityContext interface remains associated with the instance for the
 * lifetime of the instance. Note that the information that the instance
 * obtains using the EntityContext interface (such as the result of the
 * getPrimaryKey() method) may change, as the container assigns the instance
 * to different EJB objects during the instance's life cycle.</P>
 */
public interface EntityContext extends EJBContext {

  /**
   * <P>Obtain a reference to the EJB local object that is currently associated with the instance.</P>
   *
   * <P>An instance of an entity enterprise Bean can call this method only when the instance is associated
   * with an EJB local object identity, i.e. in the ejbActivate, ejbPassivate, ejbPostCreate, ejbRemove, ejbLoad,
   * ejbStore, and business methods.</P>
   *
   * <P>An instance can use this method, for example, when it wants to pass a reference to itself in a
   * method argument or result.</P>
   *
   * @return The EJB local object currently associated with the instance.
   * @exception java.lang.IllegalStateException - if the instance invokes this method while the instance
   * is in a state that does not allow the instance to invoke this method, or if the instance does not have
   * a local interface.
   */
  public EJBLocalObject getEJBLocalObject() throws IllegalStateException;

  /**
   * <P>Obtain a reference to the EJB object that is currently associated with the instance.</P>
   *
   * <P>An instance of an entity enterprise Bean can call this method only when the instance is associated
   * with an EJB object identity, i.e. in the ejbActivate, ejbPassivate, ejbPostCreate, ejbRemove, ejbLoad,
   * ejbStore, and business methods.</P>
   *
   * <P>An instance can use this method, for example, when it wants to pass a reference to itself in a method
   * argument or result.</P>
   *
   * @return The EJB object currently associated with the instance.
   * @exception java.lang.IllegalStateException - Thrown if the instance invokes this method while the instance
   * is in a state that does not allow the instance to invoke this method, or if the instance does not have a
   * remote interface.
   */
  public EJBObject getEJBObject() throws IllegalStateException;

  /**
   * <P>Obtain the primary key of the EJB object that is currently associated with this instance.</P>
   *
   * <P>An instance of an entity enterprise Bean can call this method only when the instance is associated
   * with an EJB object identity, i.e. in the ejbActivate, ejbPassivate, ejbPostCreate, ejbRemove, ejbLoad,
   * ejbStore, and business methods.</P>
   *
   * <P><B>Note:</B> The result of this method is that same as the result of getEJBObject().getPrimaryKey().</P>
   *
   * @return The primary key currently associated with the instance.
   * @exception java.lang.IllegalStateException - Thrown if the instance invokes this method while the
   * instance is in a state that does not allow the instance to invoke this method.
   */
  public Object getPrimaryKey() throws IllegalStateException;
}
