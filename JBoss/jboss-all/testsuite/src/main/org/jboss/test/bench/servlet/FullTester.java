package org.jboss.test.bench.servlet;

import java.net.URL;
import java.util.ArrayList;

import javax.servlet.http.HttpServletRequest;

public class FullTester {
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
	int maxClients;
	
	HttpServletRequest req;

	// only the "depth" first items of this array will be used
	public static final int nbClients[] = { 1, 10, 50, 100, 200, 500 };
	public int depth;
	public int nbTests = 0;
	int nbCalls;

	ArrayList testNames = new ArrayList();
	ArrayList testResults = new ArrayList();

	public FullTester(HttpServletRequest req) {
		
		maxClients = Integer.parseInt(req.getParameter("maxClients"));
		nbCalls = Integer.parseInt(req.getParameter("nbCalls"));
		
		this.req = req;

		depth = nbClients.length;
		for (int i = 0; i< nbClients.length; i++) if (nbClients[i] > maxClients) {
			depth = i; 
			break;
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
			if (req.getParameter("servlet") != null) {
				float[] result = testURL("http://localhost:8080/bench/servlet/SimpleServlet?dest=none");
				testNames.add("Servlet alone");
				testResults.add(result);
				nbTests++;

			}
			if (req.getParameter("servlet2SL") != null) {
				float[] result = testURL("http://localhost:8080/bench/servlet/SimpleServlet?dest=SL");
				testNames.add("Servlet calling stateless session");
				testResults.add(result);
				nbTests++;

			}
			if (req.getParameter("servlet2Entity") != null) {
				float[] result = testURL("http://localhost:8080/bench/servlet/SimpleServlet?dest=Entity");
				testNames.add("Servlet calling entity");
				testResults.add(result);
				nbTests++;
			}
		} catch (Exception e) {
			log.debug("failed", e);
		}
	}

	public float[] testURL(String url) throws Exception {

		Thread[] threads = new Thread[maxClients];
		float[] result = new float[depth];


		class Worker extends Thread {
			String url;
			int loops;

			public Worker(int loops, String url) {
				this.loops = loops;
				this.url = url;
			}

			public void run() {
				for (int i=0; i<loops; i++) {
					try {
						URL theUrl = new URL(url);
						Object obj = theUrl.getContent();

					} catch (Exception e) {
					}
				}
			}
		}

		for (int i = 0; i < depth; i++) {
			
			log.debug("Calling url " + url + " with " + nbClients[i] + " clients");
			
			int loops = nbCalls / nbClients[i];

			for (int threadNumber = 0; threadNumber < nbClients[i]; threadNumber++) {
				Worker worker = new Worker(loops, url);
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
