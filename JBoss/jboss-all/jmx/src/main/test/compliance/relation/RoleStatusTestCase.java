/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.relation;

import javax.management.relation.RoleStatus;

import junit.framework.TestCase;

/**
 * Role Status tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RoleStatusTestCase
  extends TestCase
{

  // Constants -----------------------------------------------------------------

  static int[] statii = new int[]
  {
    RoleStatus.LESS_THAN_MIN_ROLE_DEGREE,
    RoleStatus.MORE_THAN_MAX_ROLE_DEGREE,
    RoleStatus.NO_ROLE_WITH_NAME,
    RoleStatus.REF_MBEAN_NOT_REGISTERED,
    RoleStatus.REF_MBEAN_OF_INCORRECT_CLASS,
    RoleStatus.ROLE_NOT_READABLE,
    RoleStatus.ROLE_NOT_WRITABLE
  };

  static String[] statiiDesc = new String[]
  {
    "LESS_THAN_MIN_ROLE_DEGREE",
    "MORE_THAN_MAX_ROLE_DEGREE",
    "NO_ROLE_WITH_NAME",
    "REF_MBEAN_NOT_REGISTERED",
    "REF_MBEAN_OF_INCORRECT_CLASS",
    "ROLE_NOT_READABLE",
    "ROLE_NOT_WRITABLE"
  };

  // Attributes ----------------------------------------------------------------

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RoleStatusTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Make sure all the constants are different
   */
  public void testDifferent()
  {
    for (int i = 0; i < (statii.length - 1); i++)
    {
      for (int j = i + 1; j < statii.length; j++)
        if (statii[i] == statii[j])
          fail("RoleStatus constants are not unique");
    }
  }

  /**
   * Make sure all the constants are accepted
   */
  public void testRoleStatus()
  {
    RoleStatus test = new RoleStatus();
    for (int i = 0; i < statii.length; i++)
    {
       if (test.isRoleStatus(statii[i]) == false)
         fail(statiiDesc + " is not a role status");
    }
  }
}
