/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.state;

import java.util.Set;
import java.util.Iterator;
import java.util.HashSet;

/**
 * ???
 *      
 * @version <tt>$Revision: 1.4 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Test
{
   static StateMachine machine = null;
   static StateMachine.Model originalModel = null;

   static State NEW = new State(0, "NEW");
   static State INITIALIZING = new State(1, "INITIALIZING");
   static State INITIALIZED = new State(2, "INITIALIZED");
   static State STARTING = new State(3, "STARTING");
   static State STARTED = new StateAdapter(4, "STARTED") {
         public void stateChanged(StateMachine.ChangeEvent event) {
            startedGotEvent = true;
         }
      };
   static State FAILED = new StateAdapter(100, "FAILED") {
         public void stateChanged(StateMachine.ChangeEvent event) {
            failedGotEvent = true;
         }
      };
   static State FINAL = new AcceptableState(101, "FINAL") {
         public boolean isAcceptable(State state) {
            finalChecking = true;
            return false;
         }
      };

   static boolean startedGotEvent = false;
   static boolean failedGotEvent = false;
   static boolean finalChecking = false;

   public static StateMachine.Model makeClone()
   {
      StateMachine.Model model = (StateMachine.Model)originalModel.clone();
      Assert.assertTrue(model.equals(originalModel), "Clone was mutated");
      return model;
   }

   private static class Assert
   {
      public static void assertTrue(boolean rv)
      {
         assertTrue(rv, null);
      }

      public static void assertTrue(boolean rv, String msg)
      {
         if (!rv && msg != null) {
            System.out.println(rv + ": " + msg);
         }
         else {
            System.out.println(rv);
         }
      }
   }

   public static boolean canSerialize(java.io.Serializable obj)
   {
      try {
         org.jboss.util.Objects.copy(obj);
         return true;
      }
      catch (Exception e) {
         return false;
      }
   }
   
   public static void main(String[] args)
      throws Exception
   {
      System.out.println("\nTesting data structure equality...");
      Assert.assertTrue(new DefaultStateMachineModel().equals(new DefaultStateMachineModel()));
      
      Set set;

      Set setA = new HashSet();
      setA.add(FAILED);
      
      Set setB = new HashSet();
      setB.add(FAILED);
      
      Set setC = new HashSet();
      setC.add(FAILED);
      setC.add(NEW);

      Set setD = new HashSet();
      setD.add(FAILED);
      setD.add(STARTED);
      setD.add(NEW);
      
      DefaultStateMachineModel modelA = new DefaultStateMachineModel();
      modelA.addState(NEW);
      DefaultStateMachineModel modelB = new DefaultStateMachineModel();
      modelB.addState(NEW);
      DefaultStateMachineModel modelC = new DefaultStateMachineModel();
      modelC.addState(FINAL);
      DefaultStateMachineModel modelD = new DefaultStateMachineModel();
      modelD.addState(FINAL);
      modelD.addState(NEW);
      modelD.setInitialState(FINAL);

      DefaultStateMachineModel modelA1 = new DefaultStateMachineModel();
      modelA1.addState(NEW, setA);
      DefaultStateMachineModel modelB1 = new DefaultStateMachineModel();
      modelB1.addState(NEW, setB);
      DefaultStateMachineModel modelC1 = new DefaultStateMachineModel();
      modelC1.addState(FINAL, setC);
      DefaultStateMachineModel modelD1 = new DefaultStateMachineModel();
      modelD1.addState(FINAL, setD);
      modelD1.addState(NEW);
      modelD1.setInitialState(FINAL);

      DefaultStateMachineModel modelA2 = (DefaultStateMachineModel)modelA1.clone();
      DefaultStateMachineModel modelB2 = (DefaultStateMachineModel)modelB1.clone();
      DefaultStateMachineModel modelC2 = (DefaultStateMachineModel)modelC1.clone();
      DefaultStateMachineModel modelD2 = (DefaultStateMachineModel)modelD1.clone();
      
      Assert.assertTrue(modelA.equals(modelA) == true);
      Assert.assertTrue(modelA.equals(modelB) == true);
      Assert.assertTrue(modelB.equals(modelA) == true);
      Assert.assertTrue(modelA.equals(modelC) != true);

      Assert.assertTrue(modelA1.equals(modelA1) == true);
      Assert.assertTrue(modelA1.equals(modelB1) == true);
      Assert.assertTrue(modelB1.equals(modelA1) == true);
      Assert.assertTrue(modelA1.equals(modelC1) != true);
      Assert.assertTrue(modelD1.equals(modelD1) == true);
      Assert.assertTrue(modelD1.equals(modelA1) != true);

      Assert.assertTrue(modelA.equals(modelA1) != true);
      Assert.assertTrue(modelB.equals(modelB1) != true);
      Assert.assertTrue(modelC.equals(modelC1) != true);
      Assert.assertTrue(modelD.equals(modelD1) != true);

      Assert.assertTrue(modelA1.equals(modelA2) == true);
      Assert.assertTrue(modelB1.equals(modelB2) == true);
      Assert.assertTrue(modelC1.equals(modelC2) == true);
      Assert.assertTrue(modelD1.equals(modelD2) == true);

      modelD.removeState(NEW);

      System.out.println("\nTesting serializaion...");
      Assert.assertTrue(canSerialize(new State(0, "")));
      Assert.assertTrue(canSerialize(new StateAdapter(0, "")));
      Assert.assertTrue(canSerialize(new AcceptableState(0, "") { public boolean isAcceptable(State state) { return false; } }));
      Assert.assertTrue(canSerialize(new DefaultStateMachineModel()));
      
      System.out.println("\nSetting up model for tests...");

      DefaultStateMachineModel model = new DefaultStateMachineModel();
      
      Assert.assertTrue(model.equals(new DefaultStateMachineModel()) == true);
      Assert.assertTrue(model.equals((StateMachine.Model)model.clone()) == true);
   
      set = model.addState(NEW, INITIALIZING);
      Assert.assertTrue(((set == null) == true), "1");

      Assert.assertTrue(model.equals(new DefaultStateMachineModel()) != true);
      Assert.assertTrue(model.equals((StateMachine.Model)model.clone()) == true);
      
      model.addState(INITIALIZING, new State[] { INITIALIZED, FAILED });
      model.addState(INITIALIZED, new State[] { STARTING, FAILED });
      model.addState(STARTING, INITIALIZED);
      
      Assert.assertTrue(model.equals((StateMachine.Model)model.clone()) == true);

      // test set replacement returns
      model.addState(STARTED, INITIALIZED); // invalid state
      set = model.addState(STARTED, STARTING); // this is what we want
      Assert.assertTrue(set.size() == 1 && set.contains(INITIALIZED));

      Assert.assertTrue(model.equals((StateMachine.Model)model.clone()) == true);
      
      model.addState(FINAL);

      Assert.assertTrue(model.equals((StateMachine.Model)model.clone()) == true);
      
      Set mostStates = new HashSet(model.states());
      mostStates.remove(NEW); // new can only transition to INITIALIZED, not FAILED
      mostStates.remove(FAILED); // can not accept outselves
      model.addState(FAILED, mostStates);
      
      model.setInitialState(NEW);

      originalModel = (DefaultStateMachineModel)org.jboss.util.Objects.copy(model);
      System.out.println("Original model: " + originalModel);

      System.out.println("\nTesting clonability of model...");

      StateMachine.Model aModel;

      Assert.assertTrue(model.equals(makeClone()) == true);

      aModel = (StateMachine.Model)model.clone();
      Assert.assertTrue(model.equals(aModel) == true);
      Assert.assertTrue(aModel.equals(model) == true);

      aModel.clear();

      Assert.assertTrue(model.equals(aModel) != true);

      aModel = (StateMachine.Model)model.clone();
      Assert.assertTrue(model.equals(aModel) == true);
      
      aModel.removeState(FINAL);
      Assert.assertTrue(model.equals(aModel) != true);
      
      aModel = (StateMachine.Model)model.clone();
      Assert.assertTrue(model.equals(aModel) == true);
      
      aModel.addState(new State(FINAL.getValue(), "NEW FINAL"));

      Assert.assertTrue(model.equals(aModel) == true);

      machine = new StateMachine(makeClone());
      System.out.println(machine);
      System.out.println();
      
      test("new machine");

      Assert.assertTrue(finalChecking, "Acceptable State broken");
      Assert.assertTrue(startedGotEvent, "ChangeListener broken");
      Assert.assertTrue(failedGotEvent, "ChangeListener broken");
      
      machine.reset();
      test("reset");

      machine = new StateMachine(makeClone());
      // System.out.println("Prototype model: " + model);
      // System.out.println("Machine model: " + machine.getModel());
      test("model cloning");

      aModel = makeClone();
      aModel.removeState(FAILED);
      // System.out.println("Prototype model: " + model);

      machine = new StateMachine(aModel);
      try {
         test("model cloning with removal");
      }
      catch (IllegalStateException e) {
         Assert.assertTrue(e.getMessage().equals("State must be STARTING; cannot accept state: FAILED; state=INITIALIZED"));
      }

      machine = new StateMachine(makeClone());
      // System.out.println("Prototype model states: " + model.states());
      test("model cloning after removal");
      
      // test exception handling
      machine = new StateMachine(makeClone());

      // change listener
      machine.addChangeListener(new StateMachine.ChangeListener() {
            public void stateChanged(StateMachine.ChangeEvent event) {
               throw new RuntimeException("ChangeListener");
            }
         });

      try {
         machine.transition(INITIALIZING);
         Assert.assertTrue(false);
         // should not make it here
      }
      catch (RuntimeException e) {
         Assert.assertTrue(e.getMessage().equals("ChangeListener"));
         Assert.assertTrue(machine.getCurrentState().equals(INITIALIZING));
      }

      machine = new StateMachine(makeClone());
      
      // acceptable state
      State state = new AcceptableState(100, "FAILED") {
            public boolean isAcceptable(State state) {
               throw new RuntimeException("Accetable");
            }
         };

      // System.out.println("New FAILED state: " + state + "(" + state.toIdentityString() + ")");
      
      aModel = machine.getModel();
      // System.out.println("Most states: " + mostStates);
      
      set = aModel.addState(state, mostStates); // will replace previous state with same value
      // System.out.println("Removed states: " + set);
      // System.out.println("new states: " + aModel.states());

      machine.transition(INITIALIZING);
      Assert.assertTrue(machine.getCurrentState().equals(INITIALIZING));
      machine.transition(FAILED);
      Assert.assertTrue(machine.getCurrentState().equals(FAILED));
      
      try {
         machine.transition(FINAL);
         // should not make it here
      }
      catch (Exception e) {
         Assert.assertTrue(e.getMessage().equals("Accetable"));
         Assert.assertTrue(machine.getCurrentState().equals(FAILED));
      }

      System.out.println("\nDone.");
   }

   public static void dumpState(State state)
   {
      Set acceptable = machine.getModel().states();
      System.out.println(state + " accepts " + acceptable);
   }
   
   public static void dumpStates(Set set)
   {
      Iterator iter = set.iterator();
      while (iter.hasNext()) {
         dumpState((State)iter.next());
      }
   }
   
   public static void test(String name)
   {
      System.out.println("\nTesting " + name + "...");

      StateMachine.Model model = machine.getModel();
      // System.out.println("Using model: " + model);

      Assert.assertTrue(model.getInitialState().equals(NEW));
      Assert.assertTrue(machine.getCurrentState().equals(NEW));
      
      // dumpStates(machine.getModel().states());

      // dumpState();

      // try some valid state changes
      machine.transition(INITIALIZING);
      Assert.assertTrue(machine.getCurrentState().equals(INITIALIZING));
      machine.transition(INITIALIZED);
      Assert.assertTrue(machine.getCurrentState().equals(INITIALIZED));

      // now for an invalid state change
      try {
         machine.transition(NEW);
         Assert.assertTrue(false);
      }
      catch (IllegalStateException e) {
         Assert.assertTrue(machine.getCurrentState().equals(INITIALIZED));
      }

      // now for an invalid when we are in a final state
      machine.transition(FAILED);
      Assert.assertTrue(machine.getCurrentState().equals(FAILED));

      machine.transition(FINAL);
      Assert.assertTrue(machine.getCurrentState().equals(FINAL));
      
      try {
         machine.transition(NEW);
         Assert.assertTrue(false);
      }
      catch (IllegalStateException e) {
         Assert.assertTrue(machine.getCurrentState().equals(FINAL));
      }
   }
   
   public static void dumpState()
   {
      System.out.print("Current state: ");
      dumpState(machine.getCurrentState());
   }
}
