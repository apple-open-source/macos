package org.jboss.test.bench.servlet;

import java.util.ArrayList;

import javax.servlet.http.HttpServletRequest;

import javax.naming.Context;
import javax.naming.InitialContext;

import org.jboss.test.bench.interfaces.MySession;
import org.jboss.test.bench.interfaces.MySessionHome;
import org.jboss.test.bench.interfaces.SimpleEntity;
import org.jboss.test.bench.interfaces.SimpleEntityHome;
import org.jboss.test.bench.interfaces.ComplexEntity;
import org.jboss.test.bench.interfaces.ComplexEntityHome;
import org.jboss.test.bench.interfaces.AComplexPK;



public class EJBTester {
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
	int maxClients;
	Context ctx;
	
	HttpServletRequest req;

	// only the "depth" first items of this array will be used
	public static final int nbClients[] = { 1, 10, 50, 100, 200, 500 };
	public int depth;
	public int nbTests = 0;
	int nbCalls;
	int dataSize = 1024;

	ArrayList testNames = new ArrayList();
	ArrayList testResults = new ArrayList();

	public EJBTester(HttpServletRequest req) {
		
		maxClients = Integer.parseInt(req.getParameter("maxClients"));
		nbCalls = Integer.parseInt(req.getParameter("nbCalls"));
		
		this.req = req;

		depth = nbClients.length;
		for (int i = 0; i< nbClients.length; i++) if (nbClients[i] > maxClients) {
			depth = i; 
			break;
		}

		try {
			
			System.setProperty("java.naming.factory.initial","org.jnp.interfaces.NamingContextFactory");
			System.setProperty("java.naming.provider.url","localhost");
			System.setProperty("java.naming.factory.url.pkgs","org.jboss.naming;");
			
			ctx = new InitialContext();
			
		} catch (Exception e) {
			log.debug("failed", e);
		}
	}

	public String getTestName(int i) {
		return (String)testNames.get(i);
	}

	public float getTestResult(int i, int j) {
		return ((float[])testResults.get(i))[j];
	}

	public void test() {
		try {
			if (req.getParameter("createSimpleEntity") != null) {
				SimpleEntityHome home;
				float[] result;
				
				home = (SimpleEntityHome)ctx.lookup("SimpleEntity");
				result = testSimpleCreateEntity(home);
				testNames.add("Simple Entity Bean creation (optimized)");
				testResults.add(result);
				nbTests++;
				
				home = (SimpleEntityHome)ctx.lookup("NonOptSimpleEntity");
				result = testSimpleCreateEntity(home);
				testNames.add("Simple Entity Bean creation (serialized)");
				testResults.add(result);
				nbTests++;
			}
			if (req.getParameter("createComplexEntity") != null) {
				ComplexEntityHome home;
				float[] result;
				
				home = (ComplexEntityHome)ctx.lookup("ComplexEntity");
				result = testComplexCreateEntity(home);
				testNames.add("Complex Entity Bean creation (optimized)");
				testResults.add(result);
				nbTests++;
				
				home = (ComplexEntityHome)ctx.lookup("NonOptComplexEntity");
				result = testComplexCreateEntity(home);
				testNames.add("Complex Entity Bean creation (serialized)");
				testResults.add(result);
				nbTests++;
			}
			if (req.getParameter("readEntity") != null) {
				SimpleEntityHome home;
				float[] result;
				
				home = (SimpleEntityHome)ctx.lookup("SimpleEntity");
				result = readEntity(home);
				testNames.add("Read-only call on an entity bean (optimized)");
				testResults.add(result);
				nbTests++;
				
				home = (SimpleEntityHome)ctx.lookup("NonOptSimpleEntity");
				result = readEntity(home);
				testNames.add("Read-only call on an entity bean (serialized)");
				testResults.add(result);
				nbTests++;
			}
			if (req.getParameter("writeEntity") != null) {
				ComplexEntityHome home;
				float[] result;
				
				home = (ComplexEntityHome)ctx.lookup("ComplexEntity");
				result = writeEntity(home);
				testNames.add("Write call to entity (optimized)");
				testResults.add(result);
				nbTests++;
				
				home = (ComplexEntityHome)ctx.lookup("NonOptComplexEntity");
				result = writeEntity(home);
				testNames.add("Write call to entity (serialized)");
				testResults.add(result);
				nbTests++;
			}
			if (req.getParameter("callSF") != null) {
				MySessionHome home;
				float[] result;
				
				home = (MySessionHome)ctx.lookup("StatefulSession");
				result = callSession(home);
				testNames.add("Call to stateful session (optimized)");
				testResults.add(result);
				nbTests++;
			}
			if (req.getParameter("callSL") != null) {
				MySessionHome home;
				float[] result;
								
				home = (MySessionHome)ctx.lookup("StatelessSession");
				result = callSession(home);
				testNames.add("Call to stateless session (optimized)");
				testResults.add(result);
				nbTests++;
			}
			
		} catch (Exception e) {
			log.debug("failed", e);
		}
	}

	public float[] testSimpleCreateEntity(SimpleEntityHome home) throws Exception {

		Thread[] threads = new Thread[maxClients];
		float[] result = new float[depth];

		class Worker extends Thread {
			int startId = 0;
			int numBeans = 0;
			SimpleEntityHome home;
			
			public Worker(int startId, int numBeans, SimpleEntityHome home) {
				this.startId = startId;
				this.numBeans = numBeans;
				this.home = home;
			}

			public void run() {
				for (int i=0; i<numBeans; i++) {
					try {
						SimpleEntity bean = home.create(startId + i);
					} catch (Exception e) {
					}
				}
			}
		}

		for (int i = 0; i < depth; i++) {
			
			log.debug("Testing simple bean creation with " + nbClients[i] + " clients");
			
			int numBeans = nbCalls / nbClients[i];

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				Worker worker = new Worker(i * nbCalls + threadNumber * numBeans, numBeans, home);
				threads[threadNumber] = worker;
			}
			
			long start = System.currentTimeMillis();
			
			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				threads[threadNumber].start();
			}

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				try {
					threads[threadNumber].join();
				} catch (InterruptedException e) {
					// ignore
				}
			}

			long stop = System.currentTimeMillis();
			
			result[i] = ((float)(stop-start)) / (numBeans * nbClients[i]);
			
		}
		
		return result;
	}

	public float[] testComplexCreateEntity(ComplexEntityHome home) throws Exception {

		Thread[] threads = new Thread[maxClients];
		float[] result = new float[depth];

		class Worker extends Thread {
			long aLong;
			double aDouble;
			int numBeans = 0;
			ComplexEntityHome home;
			String aString = new String(new char[dataSize]);
			
			public Worker(long aLong, double aDouble, int numBeans, ComplexEntityHome home) {
				this.aLong = aLong;
				this.aDouble = aDouble;
				this.numBeans = numBeans;
				this.home = home;
			}

			public void run() {
				for (int i=0; i<numBeans; i++) {
					try {
						ComplexEntity bean = home.create(true, i, aLong, aDouble, aString);
					} catch (Exception e) {
					}
				}
			}
		}

		for (int i = 0; i < depth; i++) {
			
			log.debug("Testing complex bean creation with " + nbClients[i] + " clients");
			
			int numBeans = nbCalls / nbClients[i];

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {

				Worker worker = new Worker((long)i, (double)threadNumber, numBeans, home);
				threads[threadNumber] = worker;
			}
			
			long start = System.currentTimeMillis();
			
			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				threads[threadNumber].start();
			}

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				try {
					threads[threadNumber].join();
				} catch (InterruptedException e) {
					// ignore
				}
			}

			long stop = System.currentTimeMillis();
			
			result[i] = ((float)(stop-start)) / (numBeans * nbClients[i]);
			
		}
		
		return result;
	}

	public float[] readEntity(SimpleEntityHome home) throws Exception {
		Thread[] threads = new Thread[maxClients];
		float[] result = new float[depth];

		class Worker extends Thread {
			int loops;
			SimpleEntity bean;
			
			public Worker(int beanId, SimpleEntityHome wHome, int loops) throws Exception {
				this.loops = loops;
				
				try { 
					bean = wHome.findByPrimaryKey(new Integer(beanId));
				} catch (Exception e) {
					bean = wHome.create(beanId);
				}
			}

			public void run() {
				for (int i=0; i<loops; i++) {
					try {
						int field = bean.getField();
					} catch (Exception e) {
					}
				}
			}
		}

		for (int i = 0; i < depth; i++) {
			
			log.debug("Testing read-only call on simple entity with " + nbClients[i] + " clients");
			
			int loops = nbCalls / nbClients[i];

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {

				Worker worker = new Worker(threadNumber, home, loops);
				threads[threadNumber] = worker;
			}
			
			long start = System.currentTimeMillis();
			
			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				threads[threadNumber].start();
			}

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				try {
					threads[threadNumber].join();
				} catch (InterruptedException e) {
					// ignore
				}
			}

			long stop = System.currentTimeMillis();
			
			result[i] = ((float)(stop-start)) / (loops * nbClients[i]);
			
		}
		
		return result;
	}

	public float[] writeEntity(ComplexEntityHome home) throws Exception {
		Thread[] threads = new Thread[maxClients];
		float[] result = new float[depth];

		class Worker extends Thread {
			int loops;
			String otherField = new String(new char[dataSize]);
			ComplexEntity bean;
			
			public Worker(int beanId, ComplexEntityHome wHome, int loops) throws Exception {
				this.loops = loops;
				
				try { 
					bean = wHome.findByPrimaryKey(new AComplexPK(true, beanId, (long)0, (double)0, "empty"));
				} catch (Exception e) {
					bean = wHome.create(true, beanId, (long)0, (double)0, "empty");
				}
			}

			public void run() {
				for (int i=0; i<loops; i++) {
					try {
						bean.setOtherField(otherField + i);
					} catch (Exception e) {
					}
				}
			}
		}

		for (int i = 0; i < depth; i++) {
			
			log.debug("Testing call with db write on complex entity with " + nbClients[i] + " clients");
			
			int loops = nbCalls / nbClients[i];

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {

				Worker worker = new Worker(i * maxClients + threadNumber, home, loops);
				threads[threadNumber] = worker;
			}
			
			long start = System.currentTimeMillis();
			
			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				threads[threadNumber].start();
			}

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				try {
					threads[threadNumber].join();
				} catch (InterruptedException e) {
					// ignore
				}
			}

			long stop = System.currentTimeMillis();
			
			result[i] = ((float)(stop-start)) / (loops * nbClients[i]);
			
		}
		
		return result;
	}

	public float[] callSession(MySessionHome home) throws Exception {
		Thread[] threads = new Thread[maxClients];
		float[] result = new float[depth];

		class Worker extends Thread {
			int loops;
			MySession bean;
			
			public Worker(MySessionHome wHome, int loops) throws Exception {
				this.loops = loops;
				
				bean = wHome.create();
			}

			public void run() {
				for (int i=0; i<loops; i++) {
					try {
						int res = bean.getInt();
					} catch (Exception e) {
						log.debug("failed", e);
					}
				}
			}
		}

		for (int i = 0; i < depth; i++) {
			
			log.debug("Testing call to session bean " + nbClients[i] + " clients");
			
			int loops = nbCalls / nbClients[i];

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {

				Worker worker = new Worker(home, loops);
				threads[threadNumber] = worker;
			}
			
			long start = System.currentTimeMillis();
			
			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				threads[threadNumber].start();
			}

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				try {
					threads[threadNumber].join();
				} catch (InterruptedException e) {
					// ignore
				}
			}

			long stop = System.currentTimeMillis();
			
			result[i] = ((float)(stop-start)) / (loops * nbClients[i]);
			
		}
		
		return result;
	}

		

}
