/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.state;

import java.io.Serializable;

import java.util.List;
import java.util.ArrayList;
import java.util.Set;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Collections;
import java.util.EventListener;
import java.util.EventObject;

import org.jboss.util.NullArgumentException;
import org.jboss.util.CloneableObject;
import org.jboss.util.PrettyString;

/**
 * A generalization of a programmable finite-state machine (with a twist).
 *
 * <p>A state machine is backed up by a {@link Model}
 *    which simply provides data encapsulation.  The machine starts
 *    in the initial state, which must not be null.  Care must be
 *    taken to ensure that the machine state is not corrupted
 *    due to invalid modifications of the model.  Best to leave the
 *    model alone once the machine has been created.
 * 
 * <p>Provides change notification via {@link ChangeListener} objects.
 *    When a listener throws an exception it will not corrupt the state
 *    machine; it may however corrupt any application state which is
 *    dependent on the listener mechanism.  For this reason listeners
 *    should handle exceptions which might be thrown or the client
 *    application should handle recovery of such corruption by catching
 *    those undeclared exceptions.
 *
 * <p>State instances which implement {@link Acceptable} will
 *    be consulted to determine if a state is acceptable.  If such a
 *    state throws an exception the state of the machine will not change.
 *    The exception will be propagated to the client application for processing.
 *
 * <p>Durring state change events, such as acceptting and resetting,
 *    if the previous and/or current state objects implement
 *    {@link ChangeListener} they will be notified in that order.
 *
 * <p>State machine is not synchronized.  Use {@link #makeSynchronized}
 *    to make a machine thread safe.
 *
 * <p>Example of how to program a state machine:
 * <pre>
 * <code>
 *    // Create some states
 *    State NEW = new State(0, "NEW");
 *    State INITALIZEING = new State(1, "INITALIZING");
 *    State INITIALIZED = new State(2, "INITIALIZED");
 *    State STARTING = new State(3, "STARTING");
 *    State STARTED = new State(4, "STARTED");
 *    State FAILED = new State(5, "FAILED");
 *    
 *    // Create a model for the state machine
 *    StateMachine.Model model = new DefaultStateMachineModel();
 *
 *    // Add some state mappings
 *    model.addState(NEW, INITIALIZING);
 *    model.addState(INITIALIZING, new State[] { INITIALIZED, FAILED });
 *    model.addState(INITIALIZED, new State[] { STARTING });
 *    model.addState(STARTING, new State[] { STARTED, FAILED });
 *
 *    // These states are final (they do not accept any states)
 *    model.addState(STARTED);
 *    model.addState(FAILED);
 *
 *    // Set the initial state
 *    model.setInitialState(NEW);
 *
 *    // Create the machine
 *    StateMachine machine = new StateMachine(model);
 * </code>
 * </pre>
 *
 * <p>Once you have created a StateMachine instance, using it is simple:
 * <pre>
 * <code>
 *    // Change to the INITIALIZING state
 *    machine.transition(INITIALIZING);
 *
 *    // Change to the INITIALIZED state
 *    machine.transition(INITIALIZED); *
 *
 *    // Try to change to an invalid state:
 *    try {
 *       // As programmed, the INITIALIZED state does not accept the NEW
 *       // state, it only accepts the STARTING state.
 *       machine.transition(NEW);
 *    }
 *    catch (IllegalStateException e) {
 *       // Do something with the exception; The state of the machine is
 *       // still INITIALIZED.
 *    }
 *
 *    // Transition to a final state
 *    machine.transition(STARTING);
 *    machine.transition(FAILED);
 *
 *    // Any attempt to transition to any state will fail, the FAILED is
 *    // a final state, as it does not accept any other states.
 *
 *    // Reset the machine so we can use it again
 *    machine.reset();
 *
 *    // The state of the machine is now the same as it was when the
 *    // state machine was first created (short of any added change
 *    // listeners... they do not reset).
 * </code>
 * </pre>
 * 
 * @version <tt>$Revision: 1.5 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class StateMachine
   extends CloneableObject
   implements Serializable
{
   /** The data model for the machine. */
   protected final Model model;

   /** The list of change listeners which are registered. */
   protected final List changeListeners;

   /**
    * Construct a state machine with the given model.
    *
    * @param model   The model for the machine; must not be null.
    */
   public StateMachine(final Model model)
   {
      if (model == null)
         throw new NullArgumentException("model");

      this.model = model;
      this.changeListeners = new ArrayList();
      
      // Set the current state to the initial state
      State initialState = model.getInitialState();
      if (initialState == null)
         throw new IllegalArgumentException("Model initial state is null");

      reset();
   }

   /** For sync and immutable wrappers. */
   private StateMachine(final Model model, final boolean hereForSigChangeOnly)
   {
      // must be initialized (they are final), but never used.
      this.model = model;
      this.changeListeners = null;
   }

   /**
    * Returns a human readable snapshot of the current state of the machine.
    */
   public String toString()
   {
      StringBuffer buff = new StringBuffer(super.toString()).append(" {").append("\n");

      buff.append("  Model:\n");
      model.appendPrettyString(buff, "    ").append("\n");
      buff.append("  Change listeners: ").append(changeListeners).append("\n}");
      
      return buff.toString();
   }
   
   /**
    * Return the model which provides data encapsulation for the machine.
    *
    * @return  The model for the machine.
    */
   public Model getModel()
   {
      return model;
   }
   
   /**
    * Returns the current state of the machine.
    *
    * @see Model#getCurrentState
    * @see StateMachine#getModel
    *    
    * @return The current state; will not be null.
    */
   public State getCurrentState()
   {
      return model.getCurrentState();
   }

   /**
    * Returns the initial state of the machine.
    *
    * @see Model#getInitialState
    * @see StateMachine#getModel
    *
    * @return The current state; will not be null.
    */
   public State getInitialState()
   {
      return model.getInitialState();
   }
   
   /**
    * Provides the interface for dynmaic state acceptability.
    *
    * <p>State instances which implement this interface will be asked
    *    if a state is acceptable before looking at the current states
    *    acceptable state list.
    */
   public static interface Acceptable
   {
      /**
       * Check if the given state is an acceptable transition from this state.
       *
       * @param state   The state to determine acceptability; must not be null.
       * @return        True if the state is acceptable, else false.
       */
      boolean isAcceptable(State state);
   }
   
   /**
    * Check if the current state can accept a transition to the given state.
    *
    * <p>If the current state is final or does not list the given state as
    *    acceptable, then machine can not make the transition.
    *    The only exception to this rule is if the current state implements
    *    {@link Acceptable}; then the state is asked to determine acceptance.
    *    If the state returns false then the acceptable list will be consulted
    *    to make the final descision.
    *
    * <p>The mapped version of the state (not the version passed to accept)
    *    will be used to check acceptance.
    *
    * @param state   The state check.
    * @return        True if the given state is acceptable for transition; else false.
    *
    * @throws IllegalArgumentException   State not found in model.
    */
   public boolean isAcceptable(State state)
   {
      if (state == null)
         throw new NullArgumentException("state");

      // if the model does not contain this state or the current state is final,
      // then we can not go anywhere
      if (!model.containsState(state) || isStateFinal()) {
         return false;
      }
      
      State currentState = model.getCurrentState();

      // Do not allow the current state to accept itself
      if (state.equals(currentState)) {
         return false;
      }
      
      boolean rv = false;

      // Replace state with the mapped version
      state = model.getMappedState(state);
      
      // If the current state implements Acceptable let it have a whack
      if (currentState instanceof Acceptable) {
         rv = ((Acceptable)currentState).isAcceptable(state);
      }

      // If we still have not accepted, then check the accept list
      Set states = model.acceptableStates(model.getCurrentState());
      if (!rv && states.contains(state)) {
         rv = true;
      }
      
      return rv;
   }

   /**
    * Attempt to transition into the give state if the current state
    * can accept it.
    *
    * <p>The mapped version of the state (not the version passed to transition)
    *    will be used in the transition.
    *
    * @param state   The state to transiiton into.
    *
    * @throws IllegalStateException   State can not be accepted, current
    *                                 state is final or non-acceptable.
    */
   public void transition(final State state)
   {
      if (!isAcceptable(state)) {
         // make an informative exception message
         StringBuffer buff = new StringBuffer();
         State current = model.getCurrentState();
         
         if (isStateFinal()) {
            buff.append("Current state is final");
         }
         else {
            buff.append("State must be ");
         
            Set temp = model.acceptableStates(current);
            State[] states = (State[])temp.toArray(new State[temp.size()]);
         
            for (int i=0; i<states.length; i++) {
               buff.append(states[i].getName());

               if (i < states.length - 2)
                  buff.append(", ");
               else if (i < states.length - 1)
                  buff.append(" or ");
            }
         }
         
         buff.append("; cannot accept state: ")
            .append(state.getName())
            .append("; state=")
            .append(current.getName());
         
         throw new IllegalStateException(buff.toString());
      }

      // state is acceptable, let any listeners know about it
      changeState(state);
   }

   /**
    * Reset the state machine.
    *
    * <p>This will transition to the initial state and send change
    *    events to all listeners and to all accepting states which
    *    implement {@link ChangeListener}.  Each state will only
    *    be notified once.
    */
   public void reset()
   {
      State prev = model.getCurrentState();
      State initial = model.getInitialState();
      ChangeEvent event = changeState(initial);

      Iterator iter = model.states().iterator();
      while (iter.hasNext()) {
         State state = (State)iter.next();

         // skip previous and initial states, they have been notfied by changeStatex
         if (state.equals(prev) ||
             state.equals(initial))
         {
            continue;
         }
         
         if (state instanceof ChangeListener) {
            ((ChangeListener)state).stateChanged(event);
         }
      }
   }

   /**
    * Change the state of the machine and send change events
    * to all listeners and to the previous and new state objects
    * if they implement {@link ChangeListener}.
    */
   protected ChangeEvent changeState(State state)
   {
      // assert state != null

      // Replace state with the mapped version
      state = model.getMappedState(state);
         
      State prev = model.getCurrentState();
      model.setCurrentState(state);

      ChangeEvent event = new ChangeEvent(this, prev, state);

      // Let the previous and current states deal if they want
      if (prev instanceof ChangeListener) {
         ((ChangeListener)prev).stateChanged(event);
      }
      if (state instanceof ChangeListener) {
         ((ChangeListener)state).stateChanged(event);
      }
      
      // fire change event
      fireStateChanged(event);

      return event;
   }

   /**
    * Return all states which are final.
    *
    * @see #isStateFinal(State)
    *
    * @return An immutable set of the final states of the machine.
    */
   public Set finalStates()
   {
      Set set = new HashSet();
      Iterator iter = model.states().iterator();
      
      while (iter.hasNext()) {
         State state = (State)iter.next();
         if (isStateFinal(state)) {
            set.add(state);
         }
      }

      return Collections.unmodifiableSet(set);
   }

   /**
    * Determine if the given state is final.
    *
    * <p>Determing the finality of states which implement
    *    {@link Acceptable} is costly.
    *
    * @param state   The state to check for finality; must not be null.
    * @return        True if the state is final; else false.
    *
    * @throws IllegalArgumentException   State not found in model.
    */
   public boolean isStateFinal(final State state)
   {
      if (state == null)
         throw new NullArgumentException("state");
      
      if (!model.containsState(state)) {
         throw new IllegalArgumentException("State not found in model: " + state);
      }

      if (state instanceof Acceptable) {
         Acceptable a = (Acceptable)state;
         Iterator iter = model.states().iterator();

         //
         // jason: Could potentially cache the results for better performance.
         //        Would have to expose control of the cachability to the user.
         // 
         while (iter.hasNext()) {
            // if at least one state is acceptable then the state is not final
            if (a.isAcceptable((State)iter.next())) {
               return false;
            }
         }
      }

      Set acceptable = model.acceptableStates(state);

      if (acceptable == null) {
         return true;
      }

      return false;
   }
   
   /**
    * Determine if the current state is final.
    *
    * @param state   The state to check for finality; must not be null.
    * @return        True if the state is final; else false.
    *
    * @see #isStateFinal(State)
    */
   public boolean isStateFinal()
   {
      return isStateFinal(model.getCurrentState());
   }

   /** 
    * Check if the given state is the current state.
    *
    * @param state   The state to check.
    * @return        True if the state is the current state.
    */
   public boolean isCurrentState(final State state)
   {
      return model.getCurrentState().equals(state);
   }

   /** 
    * Check if the given state is the initial state.
    *
    * @param state   The state to check.
    * @return        True if the state is the initial state.
    */
   public boolean isInitialState(final State state)
   {
      return model.getInitialState().equals(state);
   }
   

   ///////////////////////////////////////////////////////////////////////////
   //                           Change Listeners                            //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * Add a state change listener.
    *
    * @param listener  The listener to add; must not be null
    */
   public void addChangeListener(final ChangeListener listener)
   {
      if (listener == null)
         throw new NullArgumentException("listener");

      synchronized (changeListeners) {
         if (!changeListeners.contains(listener)) {
            changeListeners.add(listener);
         }
      }
   }

   /**
    * Remove a state change listener.
    *
    * <p>If the give value is null then this is a non-operation.
    */
   public void removeChangeListener(final ChangeListener listener)
   {
      synchronized (changeListeners) {
         if (listener != null)
            changeListeners.remove(listener);
      }
   }

   /**
    * Send a change event to all listeners.
    *
    * <p>Listeners are invoked in the same order which they have been added.
    *
    * <p>This method (as well as add and remove methods) are protected
    *    from concurrent modification exceptions.
    */
   protected void fireStateChanged(final ChangeEvent event)
   {
      // assert event != null
      
      ChangeListener[] listeners;
      
      synchronized (changeListeners) {
         listeners = (ChangeListener[])
            changeListeners.toArray(new ChangeListener[changeListeners.size()]);
      }
      
      for (int i=0; i<listeners.length; i++) {
         listeners[i].stateChanged(event);
      }
   }
   
   /**
    * An event for state change notifications.
    */
   public static class ChangeEvent
      extends EventObject
   {
      public final State previous;
      public final State current;
      
      public ChangeEvent(final StateMachine source,
                         final State previous,
                         final State current)
      {
         super(source);

         this.previous = previous;
         this.current = current;
      }

      /**
       * The previous state of the machine.
       *
       * <p>This value will not be null unless the initial state
       *    of the machine implements Acceptable.  This can be used
       *    to determine when the state machine has been reset.
       *
       * @return The previous state of the machine.
       */
      public State getPreviousState()
      {
         return previous;
      }

      /**
       * The current state of the machine. ie. The state the machine
       * has just transition into and which triggered the change event.
       *
       * @return The current state of the machine; will not be null.
       */
      public State getCurrentState()
      {
         return current;
      }

      /**
       * The state machine which generated the event.
       */
      public StateMachine getStateMachine()
      {
         return (StateMachine)getSource();
      }

      public String toString()
      {
         return super.toString() +
            "{ previous=" + previous +
            ", current=" + current +
            " }";
      }
   }

   /**
    * A listener for state change events.
    */
   public static interface ChangeListener
      extends EventListener
   {
      /**
       * Invoked after a state has been changed.
       *
       * @param event  The state event, which encodes that data for the
       *               state change.
       */
      void stateChanged(ChangeEvent event);
   }


   /////////////////////////////////////////////////////////////////////////
   //                               Model                                 //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Defines the data model required by a {@link StateMachine}.
    */
   public interface Model
      extends CloneableObject.Cloneable, PrettyString.Appendable
   {
      /**
       * Add a non-final state.
       *
       * <p>Existing acceptable states will be replaced by the given states.
       *
       * <p>Acceptable states which are not registered as accepting states
       *    will be added as final states.
       *
       * <p>If the acceptable set is null, then the added state will be final.
       *
       * <p>Note, states are added based on the valid states which can be
       *    transitioned to from the given state, not on the states which
       *    accept the given state.
       *
       * <p>For example, if adding state A which accepts B and C, this means
       *    that when the machine is in state A, it will allow transitions
       *    to B or C and not from C to A or B to A (unless of course a state
       *    mapping is setup up such that C and B both accept A).
       *
       * @param state        The accepting state; must not be null.
       * @param acceptable   The valid acceptable states; must not contain null elements.
       */
      Set addState(State state, Set acceptable);

      /**
       * Add a non-final state.
       *
       * @param state        The accepting state; must not be null.
       * @param acceptable   The valid acceptable states; must not contain null elements.
       *
       * @see #addState(State,Set)
       */
      Set addState(State state, State[] acceptable);

      /**
       * Add a final state.
       *
       * <p>Note, if the given state implements {@link StateMachine.Acceptable} then
       *    the final determiniation of its finality will be unknown until
       *    runtime.
       *
       * @param state    The final state; must not be null.
       */
      Set addState(State state);

      /**
       * Returns the state object mapped for the given state value.
       *
       * <p>Since states with the same value are equivlent, this provides
       *    access to the actual state instance which is bound in the model.
       *
       * @param state   The state with the value of the bound state to return;
       *                null will return false.
       * @return        The bound state instance.
       *
       * @throws IllegalArgumentException  State not mapped.
       */
      State getMappedState(State state);

      /**
       * Determins if there is a mapping for the given state object.
       *
       * @param state   The state with the value of the bound state to check for;
       *                must not be null.
       * @return        True if the state is mapped; else false.
       *
       * @throws IllegalArgumentException  State not mapped.
       */
      boolean isMappedState(State state);
      
      /**
       * Set the initial state.
       *
       * <p>Does not need to validate the state, {@link StateMachine} will
       *    handle those details.
       *
       * @param state   The initial state; must not be null.
       */
      void setInitialState(State state);

      /**
       * Return the initial state which the state machine should start in.
       *
       * @return The initial state of the state machine; must not be null.
       */
      State getInitialState();

      /**
       * Set the current state.
       *
       * <p>Does not need to validate the state, {@link StateMachine} will
       *    handle those details.
       *
       * @param state   The current state; must not be null.
       */
      void setCurrentState(State state);

      /**
       * Get the current state.
       *
       * @return The current state; can be null if not used by a state machine.
       *         Once it has been given to a state machine this must not be
       *         null.
       */
      State getCurrentState();

      /**
       * Check if a give state is contained in the model.
       *
       * @param state   The state to look for.
       * @return        True if the state is contained in the model; false if not.
       */
      boolean containsState(State state);

      /**
       * Remove a state from the model.
       *
       * @param state   The state to remove.
       * @return        The acceptable states for the removed state or null.
       */
      Set removeState(State state);

      /**
       * Clear all accepting state mappings and reset the initial and current
       * state to null.
       */
      void clear();
   
      /**
       * Return an immutable set of the accepting states.
       *
       * @return A set of accepting states.
       */
      Set states();

      /**
       * Return an immutable set of the acceptable states for a given
       * accepting state.
       *
       * @param state   The accepting state to get acceptable states for; must not be null.
       * @return        A set of accepting states.
       */
      Set acceptableStates(State state);
   }

   
   /////////////////////////////////////////////////////////////////////////
   //                           Synchronization                           //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Return a synchronized state machine.
    *
    * @param machine    State machine to synchronize; must not be null.
    * @param mutex      The object to lock on; null to use returned instance.
    * @return           Synchronized state machine.
    */
   public static StateMachine makeSynchronized(final StateMachine machine,
                                               final Object mutex)
   {
      if (machine == null)
         throw new NullArgumentException("machine");
      
      return new StateMachine(null, true)
         {
            private Object lock = (mutex == null ? this : mutex);

            public Model getModel()
            {
               synchronized (lock) {
                  return machine.getModel();
               }
            }
            
            public State getInitailState()
            {
               synchronized (lock) {
                  return machine.getInitialState();
               }
            }

            public State getCurrentState()
            {
               synchronized (lock) {
                  return machine.getCurrentState();
               }
            }

            public boolean isInitialState(final State state)
            {
               synchronized (lock) {
                  return machine.isInitialState(state);
               }
            }
            
            public boolean isCurrentState(final State state)
            {
               synchronized (lock) {
                  return machine.isCurrentState(state);
               }
            }
            
            public boolean isAcceptable(final State state)
            {
               synchronized (lock) {
                  return machine.isAcceptable(state);
               }
            }

            public void transition(final State state)
            {
               synchronized (lock) {
                  machine.transition(state);
               }
            }

            public void reset()
            {
               synchronized (lock) {
                  machine.reset();
               }
            }

            public Set finalStates()
            {
               synchronized (lock) {
                  return machine.finalStates();
               }
            }

            public boolean isStateFinal(final State state)
            {
               synchronized (lock) {
                  return machine.isStateFinal(state);
               }
            }

            public boolean isStateFinal()
            {
               synchronized (lock) {
                  return machine.isStateFinal();
               }
            }
            
            public Object clone()
            {
               synchronized (lock) {
                  return super.clone();
               }
            }
            
            public void addChangeListener(final ChangeListener listener)
            {
               synchronized (lock) {               
                  machine.addChangeListener(listener);
               }
            }

            public void removeChangeListener(final ChangeListener listener)
            {
               synchronized (lock) {
                  machine.removeChangeListener(listener);
               }
            }
         };
   }

   /**
    * Return a synchronized state machine.
    *
    * @param machine    State machine to synchronize; must not be null.
    * @return           Synchronized state machine.
    */
   public static StateMachine makeSynchronized(final StateMachine machine)
   {
      return makeSynchronized(machine, null);
   }   


   /////////////////////////////////////////////////////////////////////////
   //                            Immutablility                            //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Return a immutable state machine.
    *
    * <p>Immutable state machines can not be transitioned or reset; methods
    *    will throw a <tt>UnsupportedOperationException</tt>.
    *
    * <p>If model is not hidden, then users can still mess up the model
    *    if they want, thus corrupting the state machine.
    *
    * @param machine    State machine to make immutable; must not be null.
    * @param hideModel  Make the model inaccessable too.
    * @return           Immutable state machine with hidden model.
    */
   public static StateMachine makeImmutable(final StateMachine machine,
                                            final boolean hideModel)
   {
      if (machine == null)
         throw new NullArgumentException("machine");
      
      return new StateMachine(machine.getModel(), true)
         {
            public Model getModel()
            {
               if (hideModel) {
                  throw new UnsupportedOperationException
                     ("Model has been hidden; state machine is immutable");
               }
               
               return super.getModel();
            }
            
            public void transition(final State state)
            {
               throw new UnsupportedOperationException
                  ("Can not transition; state machine is immutable");
            }

            public void reset()
            {
               throw new UnsupportedOperationException
                  ("Can not reset; state machine is immutable");
            }

            public void addChangeListener(final ChangeListener listener)
            {
               throw new UnsupportedOperationException
                  ("Can not add change listener; state machine is immutable");
            }

            public void removeChangeListener(final ChangeListener listener)
            {
               throw new UnsupportedOperationException
                  ("Can not remove change listener; state machine is immutable");
            }
         };
   }
      
   /**
    * Return a immutable state machine.
    *
    * <p>Immutable state machines can not be transitioned or reset; methods
    *    will throw a <tt>UnsupportedOperationException</tt>.
    *
    * @param machine    State machine to make immutable; must not be null.
    * @return           Immutable state machine.
    */
   public static StateMachine makeImmutable(final StateMachine machine)
   {
      return makeImmutable(machine, true);
   }
}
