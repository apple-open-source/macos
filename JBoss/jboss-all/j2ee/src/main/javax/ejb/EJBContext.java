/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

import java.security.*;
import java.util.Properties;
import javax.transaction.UserTransaction;

/**
 * <P>The EJBContext interface provides an instance with access to the
 * container-provided runtime context of an enterprise Bean instance.</P>
 * <P>This interface is extended by the SessionContext and EntityContext
 * interface to provide additional methods specific to the enterprise
 * Bean type.</P>
 */
public interface EJBContext {

  /**
   * <B>Deprecated.</B> <I>Use Principal getCallerPrincipal() instead.</I>
   *
   * <P>Obtain the java.security.Identity of the caller. This method is deprecated in EJB 1.1.
   * The Container is allowed to return alway null from this method. The enterprise bean should use
   * the getCallerPrincipal method instead.</P>
   *
   * @return The Identity object that identifies the caller.
   */
  public Identity getCallerIdentity();

  /**
   * Obtains the java.security.Principal of the caller.
   *
   * @return The Principal object that identifies the caller. This method never returns null.
   */
  public Principal getCallerPrincipal();

  /**
   * Obtain the enterprise bean's remote home interface.
   *
   * @return The enterprise bean's remote home interface.
   * @exception java.lang.IllegalStateException - if the enterprise bean does not have a remote home interface.
   */
  public EJBHome getEJBHome() throws IllegalStateException;

  /**
   * Obtain the enterprise bean's local home interface.
   *
   * @return The enterprise bean's local home interface.
   * @exception java.lang.IllegalStateException - if the enterprise bean does not have a local home interface.
   */
  public EJBLocalHome getEJBLocalHome() throws IllegalStateException;

  /**
   * <B>Deprecated.</B> <I>Use the JNDI naming context java:comp/env to access enterprise bean's environment.</I>
   *
   * <P>Obtain the enterprise bean's environment properties.</P>
   *
   * <P><B>Note:</B> If the enterprise bean has no environment properties this method returns an empty
   * java.util.Properties object. This method never returns null.</P>
   *
   * @return The environment properties for the enterprise bean.
   */
  public Properties getEnvironment();

  /**
   * Test if the transaction has been marked for rollback only. An enterprise bean instance can use
   * this operation, for example, to test after an exception has been caught, whether it is fruitless
   * to continue computation on behalf of the current transaction. Only enterprise beans with
   * container-managed transactions are allowed to use this method.</P>
   *
   * @return True if the current transaction is marked for rollback, false otherwise.
   * @exception java.lang.IllegalStateException - The Container throws the exception if the instance
   * is not allowed to use this method (i.e. the instance is of a bean with bean-managed transactions). 
   */
  public boolean getRollbackOnly() throws IllegalStateException;

  /**
   * Obtain the transaction demarcation interface. Only enterprise beans with bean-managed transactions
   * are allowed to to use the UserTransaction interface. As entity beans must always use container-managed
   * transactions, only session beans with bean-managed transactions are allowed to invoke this method.
   *
   * @return The UserTransaction interface that the enterprise bean instance can use for transaction demarcation.
   * @exception java.lang.IllegalStateException - The Container throws the exception if the instance is not
   * allowed to use the UserTransaction interface (i.e. the instance is of a bean with container-managed
   * transactions).
   */
  public UserTransaction getUserTransaction() throws IllegalStateException;

  /**
   * Tests if the caller has a given role.
   *
   * @param roleName - The name of the security role. The role must be one of the security roles that
   * is defined in the deployment descriptor.
   * @return True if the caller has the specified role.
   */
  public boolean isCallerInRole(String roleName);

  /**
   * <B>Deprecated.</B> <I>Use boolean isCallerInRole(String roleName) instead.</I>
   *
   * <P>Test if the caller has a given role.</P>
   *
   * <P>This method is deprecated in EJB 1.1. The enterprise bean should use the
   * isCallerInRole(String roleName) method instead.</P>
   *
   * @param role - The java.security.Identity of the role to be tested.
   * @return True if the caller has the specified role.
   */
  public boolean isCallerInRole(Identity role);

  /**
   * Mark the current transaction for rollback. The transaction will become permanently marked for rollback.
   * A transaction marked for rollback can never commit. Only enterprise beans with container-managed
   * transactions are allowed to use this method.
   *
   * @exception java.lang.IllegalStateException - The Container throws the exception if the instance is not
   * allowed to use this method (i.e. the instance is of a bean with bean-managed transactions).
   */
  public void setRollbackOnly() throws IllegalStateException;
}
