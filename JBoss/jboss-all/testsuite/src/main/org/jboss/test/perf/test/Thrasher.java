package org.jboss.test.perf.test;
// Thrasher.java
import org.jboss.test.perf.interfaces.*;

import java.util.*;

public class Thrasher implements Runnable {

  static final int ACTION_SINGLE_READ  = 0;
  static final int ACTION_SINGLE_WRITE = 1;
  static final int ACTION_BLOCK_READ   = 2;
  static final int ACTION_BLOCK_WRITE  = 3;
  static final int ACTION_FAILURE      = 4;
  static final int ACTION_COUNT        = 5;
  
  static int[] timers   = new int[ACTION_COUNT];
  static int[] counters = new int[ACTION_COUNT];

  static final String[] ACTION_LABELS = {
    "single reads ",
    "single writes",
    "block reads  ",
    "block writes ",
    "**FAILURES** ",
  };

  static final int ARG_THREADS		=  0;
  static final int ARG_ITERATIONS	=  1;
  static final int ARG_READ_PERC	=  2;
  static final int ARG_BLOCK_PERC	=  3;
  static final int ARG_BLOCK_MIN	=  4;
  static final int ARG_BLOCK_MAX	=  5;
  static final int ARG_SLEEP_MIN	=  6;
  static final int ARG_SLEEP_MAX	=  7;
  static final int ARG_ID_MIN		=  8;
  static final int ARG_ID_MAX		=  9;
  static final int ARG_COUNT		= 10;

  static final String[] ARG_LABELS = {
    "threads   ",	"How many threads to run",
    "iterations",	"How many iterations per thread",
    "read_perc ",	"What percentage of access is readonly",
    "block_perc",	"What percentage of access is block oriented",
    "block_min ",	"The minimum block size per access (inclusive)",
    "block_max ",	"The maximum block size per access (exclusive)",
    "sleep_min ",	"The minimum sleep period between accesses in ms (inclusive)",
    "sleep_max ",	"The maximum sleep period between accesses in ms (exclusive)",
    "id_min    ",	"The minimum id accessed (inclusive)",
    "id_max    ",	"The maximum id accessed (exclusive)",
  };

  static final int[] argValues = new int[ARG_COUNT];

  static private void usage() {
    System.out.println("Usage: java Thrasher <jndi-name> [-options]\n\n" + 
                       "  where options include:");
    for(int a = 0; a < ARG_COUNT; a++) {
      System.out.println("    -" + ARG_LABELS[a * 2] + 
                         " <n> \t" + ARG_LABELS[a * 2 + 1]);
    }
    System.exit(1);
  }

  public static void main(String[] args) throws Exception {

    if(args.length < 1) {
      usage();
    }
    String jndiName = args[0];

    int[] argValues = new int[ARG_COUNT];
    argValues[ARG_THREADS]    = 1;
    argValues[ARG_ITERATIONS] = 20;
    argValues[ARG_READ_PERC]  = 90;
    argValues[ARG_BLOCK_PERC] = 10;
    argValues[ARG_BLOCK_MIN]  = 2;
    argValues[ARG_BLOCK_MAX]  = 10;
    argValues[ARG_SLEEP_MIN]  = 0;
    argValues[ARG_SLEEP_MAX]  = 0;
    argValues[ARG_ID_MIN]     = 0;
    argValues[ARG_ID_MAX]     = 100;

    for(int i = 1; i < args.length; i++) {
      boolean found = false;
      for(int a = 0; a < ARG_COUNT; a++) {
        String argLabel = "-" + ARG_LABELS[a * 2].trim();
        if(args[i].equals(argLabel)) {
          found = true;
          if(++i >= args.length) {
            System.out.println("Value expected for argument: " + argLabel);
            System.exit(1);
          }
          int value;
          try {
            value = Integer.parseInt(args[i]);
          }
          catch(NumberFormatException e) {
            System.out.println("Numeric value expected for argument: " + argLabel);
            System.out.println("  error: " + e);
            System.exit(1);
            return;
          }
          argValues[a] = value;
          break;
        }
      }
      if(!found) {
        usage();
      }
    } 
    Thrasher[] thrashers = new Thrasher[argValues[ARG_THREADS]];
    {
      Thread[] threads = new Thread[argValues[ARG_THREADS]];
      for(int i = 0; i < argValues[ARG_THREADS]; i++) {
        thrashers[i] = new Thrasher(jndiName, argValues);
        threads[i] = new Thread(thrashers[i], "Thrasher[" + i + "]");
        if(i < 5 ||
           i >= argValues[ARG_THREADS] - 5) {
          System.out.println("Starting thread: " + threads[i]);
        }
        threads[i].start();
      }
      for(int i = 0; i < argValues[ARG_THREADS]; i++) {
        threads[i].join();
      }
    }
    System.out.println("Configuration:");
    for(int a = 0; a < ARG_COUNT; a++) {
      System.out.println("\t" + ARG_LABELS[a * 2] + "\t" + argValues[a]);
    }
    {
      System.out.println("Actions:");
      for(int i = 0; i < ACTION_COUNT; i++) {
        if(counters[i] != 0) {
          float avg = (float) timers[i] / counters[i];
          // round the value...
          avg = (long) (avg * 10) / 10f;
          System.out.println("\t" + counters[i] + "\t" +
                             ACTION_LABELS[i] + "\tavg:\t" + 
                             avg + "\tms/Tx");
        }
      }
    }
    System.out.println("Performance:");
    long totalTime = 0;
    for(int i = 0; i < argValues[ARG_THREADS]; i++) {
      long thrasherTime = thrashers[i]._elapsedTime;
      totalTime += thrasherTime;
      if(i < 5 ||
         i >= argValues[ARG_THREADS] - 5) {
        // only print out the first and last 5 enties...
        float msPerTx = (float) thrasherTime / argValues[ARG_ITERATIONS];
        float txPerSec = 1000 / msPerTx;
        // round the values...
        msPerTx = (long) (msPerTx * 10) / 10f;
        txPerSec = (long) (txPerSec * 1000) / 1000f;
        System.out.println("\tThread[" + i + "]: \t" + 
                           txPerSec + "\tTx/s \t" +
                           msPerTx + "\tms/Tx");
      }
    }
    {
      float msPerTx = (float) totalTime / argValues[ARG_THREADS] / argValues[ARG_ITERATIONS];
      float txPerSec = 1000 / msPerTx;
      // round the values...
      msPerTx = (long) (msPerTx * 10) / 10f;
      txPerSec = (long) (txPerSec * 1000) / 1000f;
      System.out.println("\tAverage: \t" + 
                         txPerSec + "\tTx/s \t" +
                         msPerTx + "\tms/Tx");
      float throughput = txPerSec * argValues[ARG_THREADS];
      // round the value...
      throughput = (long) (throughput * 1000) / 1000f;
      System.out.println("\tThroughput: \t" + throughput + "\tTx/s");
    }
  }

  private String _jndiName;
  private int[] _args;
  private long _elapsedTime;

  private Thrasher(String jndiName, int[] args) {
    _jndiName = jndiName;
    _args = args;
  }

  public void run() {
    try { 
      javax.naming.Context context = new javax.naming.InitialContext();
      Object ref = context.lookup("Session");
      SessionHome sessionHome = (SessionHome) ref;
      /** CHANGES: Note that WebLogic does not support
       ** Spec Compliant PortableRemoteObject way of
       ** narrow
        //(SessionHome) javax.rmi.PortableRemoteObject.narrow(ref, SessionHome.class);
       **/
      Session session = sessionHome.create(_jndiName);
      java.util.Random random = new java.util.Random();
      for(int i = 0; i < _args[ARG_ITERATIONS]; i++) {
        boolean doBlocks   = random.nextFloat() < _args[ARG_BLOCK_PERC] / 100f;
        boolean doReadonly = random.nextFloat() < _args[ARG_READ_PERC]  / 100f;
        int id = getRandom(random, _args[ARG_ID_MIN], _args[ARG_ID_MAX]);
        int blockSize = doBlocks ? getRandom(random, _args[ARG_BLOCK_MIN], _args[ARG_BLOCK_MAX]) : 0;
        long before = System.currentTimeMillis();
        int action;
        if(doBlocks) {
          if(id + blockSize > _args[ARG_ID_MAX]) {
            // if the blocksize were going to overrun the border, shift the start back
            id = _args[ARG_ID_MAX] - blockSize;
          }
          if(doReadonly) {
            session.read(id, id + blockSize);
            action = ACTION_BLOCK_READ;
          }
          else {
            session.write(id, id + blockSize);
            action = ACTION_BLOCK_WRITE;
          }
        }
        else {
          if(doReadonly) {
            session.read(id);
            action = ACTION_SINGLE_READ;
          }
          else {
            session.write(id);
            action = ACTION_SINGLE_WRITE;
          }
        }
        long after = System.currentTimeMillis();
        _elapsedTime += after - before;
        timers[action] += after - before;
        counters[action]++;
        int sleep = getRandom(random, _args[ARG_SLEEP_MIN], _args[ARG_SLEEP_MAX]);
        if(sleep != 0) {
          try {
            Thread.currentThread().sleep(sleep);
          }
          catch(InterruptedException e) {
            // continue...
          }
        }
      }
      session.remove();
    }
    catch(Exception e) {
      counters[ACTION_FAILURE]++;
      e.printStackTrace();
    }
  }

  static int getRandom(java.util.Random random, int min, int max) {
    // first get a random non-negative number
    int result = Math.abs(random.nextInt());
    // get it into the specified range
    int range = max - min;
    if(range <= 0) {
      return min;
    }
    result %= range;
    result += min;
    return result;
  }

}
