package org.jboss.test.load.test;

import java.util.Properties;
import java.util.StringTokenizer;
import java.util.Hashtable;

import org.jboss.test.JBossTestCase;

/**
* Test client. <br>
* Deployes the testbean.jar and then starts as many workers as on the
* command line given.
* After all workers are done, it prints a success or failure message.
* The return value is the number of failed workers (0=sucess, 1=one worker failed, ...)
*
* parsed parameter:
* <dl> <li> verbose - more output </li>
*      <li> nodeploy - the testbean.jar dont becomes deployed </li>
*      <li> loops - iterations each thread has to do </li>
*      <li> beans - number of beans each thread has to deal with </li>
*      <li> threads - number of threads getting started </li>
* </dl>
*
* @author <a href="mailto:daniel.schulze@telkel.com">Daniel Schulze</a>
* @version $Id: Client.java,v 1.8.2.1 2003/07/25 16:37:15 starksm Exp $
*/
public class Client extends JBossTestCase
{
   Properties param = new Properties ();;
   int exitCode = 0;

   public Client(String name)
   {
      super(name);
   }

   public Client (String[] _args) throws Exception
   {
      super("main");
      // scanning parameters
      int i = 0;
      while (i < _args.length)
      {
         StringTokenizer st = new StringTokenizer (_args[i++], "=");
         param.put (st.nextToken (),
                    st.hasMoreTokens () ? st.nextToken () : "");
      }

      System.out.println("_____________________________________________");
       // sed kicks ass
      System.out.println("jBoss, the EJB Open Source Server");
      System.out.println("Copyright (C), The jBoss Organization, 2000");
      System.out.println("_____________________________________________");
       // sed kicks ass
      System.out.println("Welcome to the Load Test v0.1");
      System.out.println("_____________________________________________");
       // sed kicks ass
      System.out.println("the following optional parameters are supported:");
      System.out.println("   loops=<number> number of test loops per thread");
      System.out.println("   beans=<number> number of beans used per thread");
      System.out.println("   threads=<number> number of threads getting started");
      System.out.println("   delay=<millisec> delay between the thread start");
      System.out.println("   verbose - gives infos about bean creation/removing");
      System.out.println("   noremove - keeps the created beans after finish");
      System.out.println("   nodeploy - does not deploy anything");
      System.out.println("       (by default the testbeans.jar from this testsuite");
      System.out.println("        is deployed (the test needs the nextgen.EnterpriseEntity)");
      System.out.println("   name=<string> name of the threads");
      System.out.println("for batch use: in case of any failure it returns != 0");
      System.out.println ();
      
      if (param.get ("nodeploy") == null)
      {
         System.out.print("Deploying test beans...");
         System.out.flush();
         deploy("../deploy/testbean.jar");
         System.out.println("done!");
      }

      exitCode = test1 ();

      //System.out.print("Undeploying test beans...");
      //deployer.undeploy("../deploy/testbean.jar");
      //System.out.println("done!");

       // sed kicks ass
      System.out.println("Test completed.");
      System.out.println("Please take the time to report us your results to the " +
                         "jboss-dev@working-dogs.com or jboss-user@working-dogs.com mailing list");
      System.out.println(" ");
      System.out.println("  jBoss version            : ");
      System.out.println("  jBoss configuration      : ");
      System.out.println("  (conf/jboss.jcml)          ");
      System.out.println("  your OS                  : ");
      System.out.println("  JDK (vm vendor/version)  : ");
      System.out.println("  DB (product/version)     : ");
      System.out.println("  Database driver (version): ");
      System.out.println("  ");
      System.out.println("Thanks in advance!");
       // sed kicks ass
      System.out.println("note: I guess the testbeans are still deployed.");

      System.exit (exitCode);
   }

   private int test1 ()
   {
      Hashtable config = new Hashtable ();
      

      config.put ("verbose", new Boolean (param.getProperty ("verbose") != null));
      config.put ("noremove", new Boolean (param.getProperty ("noremove") != null));

      int beans = 5;
      try {
      beans = Integer.parseInt (param.getProperty ("beans"));
      } catch (Exception _e) {
         System.out.println("no (or wrong) number of beans (per thread) specified. using default: " + beans);
      }
      config.put ("beans", new Integer (beans));

      int loops = 100;
      try {
      loops = Integer.parseInt (param.getProperty ("loops"));
      } catch (Exception _e){
         System.out.println("no (or wrong) number of loops specified. using default: " + loops);
      }
      config.put ("loops", new Integer (loops));

      config.put ("name", param.getProperty ("name", "daniel") );

      int threads = 50;
      try {
      threads = Integer.parseInt (param.getProperty ("threads"));
      } catch (Exception _e){
         System.out.println("no (or wrong) thread number specified. using default: " + threads);
      }
      int delay = 1000;
      try {
      delay = Integer.parseInt (param.getProperty ("delay"));
      } catch (Exception _e){
         System.out.println("no (or wrong) delay (millisec between each thread start) specified. using default: " + delay);
      }
      System.out.println ("start test1 with "+threads+" threads, "+beans+" beans per thread in "+loops+" loops.");
      System.out.println("------------------------------------------------------");


      ThreadGroup threadGroup = new ThreadGroup ("workers");
      Worker[] workers = new Worker[threads];

      long start = System.currentTimeMillis();
      // create and start threads...
      for (int i = 0; i < threads; ++i)
      {
         Hashtable cfg = (Hashtable)config.clone ();
         cfg.put ("number", new Integer (i));
         workers[i] = new Worker (threadGroup, cfg);
         workers[i].start ();
         // because of problems with Windows 2000...
         // (on W2000 the server couldnt serve this many connection
         //  requests (jndi) in such quick sequence)
         // .. this hack:
         try
         {
            Thread.currentThread ().sleep (delay);
         } catch (InterruptedException _ie)
         { // shoudnt happen...
         }
      }
      
      // wait for all threads to finish... (is this the most elegant way?!)
      try
      {
         Thread me = Thread.currentThread ();
         while (threadGroup.activeCount () > 0)
            me.sleep (5000L);
      }
      catch (InterruptedException _ie){
         System.out.print ("Main thread interrupted?!");
      }
      long timeSum = (System.currentTimeMillis() - start)/1000L;
      
      // make statistic...
      int failed = 0;
      int tx = 0;
      for (int i = 0; i < threads; ++i)
      {
         Hashtable cfg = workers[i].getConfig ();
         if (((Boolean)cfg.get("failed")).booleanValue ())
            ++failed;

         tx += ((Integer)cfg.get("transactions")).intValue ();  
         //timeAcc += ((Long)cfg.get("time")).longValue ();
      }
      long txps = tx/timeSum;
      

      System.out.println("------------------------------------------------------");
      if (failed > 0)
      {
         System.out.println("The Test didnt succeed completly :-(");
         System.out.println("in " + failed + " threads occurred an error.");
      }
      else
      {
         System.out.println("Congratulations the test succeeded! :-)");
         System.out.println("All threads finished clean.");
      }
      
      
      long h = timeSum/3600L;
      long m = (timeSum-h*3600L)/60L;
      long s = timeSum-h*3600L-m*60L;
      System.out.println("");
      System.out.println("statistic: "+tx+" transactions in "+h+" h "+m+" min "+s+" s.");
      System.out.println("             transactions per second (average): "+txps);
      return failed;
   }



   public static void main (String[] _args) throws Exception
   {
      new Client (_args);
   }


}
