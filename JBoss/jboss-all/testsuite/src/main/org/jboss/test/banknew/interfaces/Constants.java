/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.banknew.interfaces;

/**
 * Constants for banknew test example
 *
 * @author Andreas Schaefer
 * @version $Revision: 1.3 $
 **/
public class Constants {
   
   // Constants -----------------------------------------------------
   
   public static final int CHECKING = 0;
   public static final int SAVING = 1;
   public static final int MONEY_MARKET = 2;
   
   public static final int INITIAL_DEPOSIT = 0;
   public static final int DEPOSIT = 1;
   public static final int WITHDRAW = 2;
   public static final int FINAL_WITHDRAW = 3;
   
   public static final int ONE_SECOND = 1000;
   public static final int ONE_MINUTE = 60000;
   public static final int ONE_HOUR = 3600000;
   public static final int ONE_DAY = 86400000;
}

/*
 *   $Id: Constants.java,v 1.3 2002/05/07 01:31:36 schaefera Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: Constants.java,v $
 *   Revision 1.3  2002/05/07 01:31:36  schaefera
 *   Added the support of ".ant.properties" files to enable clients to adjust
 *   settings w/o changing any files added to CVS.
 *   Also the two marathon scripts are adjusted to this and also start the
 *   reports generation at the end.
 *
 *   Revision 1.2  2002/05/04 01:08:27  schaefera
 *   Added new Stats classes (JMS related) to JSR-77 implemenation and added the
 *   bank-new test application but this does not work right now properly but
 *   it is not added to the default tests so I shouldn't bother someone.
 *
 *   Revision 1.1.2.2  2002/04/29 21:05:17  schaefera
 *   Added new marathon test suite using the new bank application
 *
 *   Revision 1.1.2.1  2002/04/17 05:07:24  schaefera
 *   Redesigned the banknew example therefore to a create separation between
 *   the Entity Bean (CMP) and the Session Beans (Business Logic).
 *   The test cases are redesigned but not finished yet.
 *
 */
