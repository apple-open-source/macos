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
 * A helper state for instances which need to be
 * {@link StateMachine.Acceptable} and/or {@link StateMachine.ChangeListener}s.
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class StateAdapter
   extends State
   implements StateMachine.Acceptable, StateMachine.ChangeListener
{
   public StateAdapter(final int value, final String name)
   {
      super(value, name);
   }

   public StateAdapter(final int value)
   {
      super(value);
   }

   /**
    * Always returns false.
    *
    * @return  False;
    */
   public boolean isAcceptable(final State state) { return false; }

   public void stateChanged(final StateMachine.ChangeEvent event) {}
}
