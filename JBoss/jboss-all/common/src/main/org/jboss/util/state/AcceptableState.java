/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.state;

/**
 * An abstract state to build dynamicily state acceptabile states.
 *
 * <p>Sub-classes must implement
 *    {@link StateMachine.Acceptable#isAcceptable(State)}.
 *      
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class AcceptableState
   extends State
   implements StateMachine.Acceptable
{
   public AcceptableState(final int value, final String name)
   {
      super(value, name);
   }

   public AcceptableState(final int value)
   {
      super(value);
   }
}
