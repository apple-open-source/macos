/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */


import javax.jms.TopicPublisher;
import javax.jms.QueueSender;
import javax.jms.TopicSession;
import javax.jms.QueueSession;
import javax.jms.TextMessage;
import javax.jms.MessageConsumer;
import javax.jms.TopicSubscriber;
import javax.jms.QueueReceiver;
import javax.jms.TopicConnectionFactory;
import javax.jms.BytesMessage;
import javax.jms.QueueConnectionFactory;
import javax.jms.TopicConnection;
import javax.jms.QueueConnection;
import javax.jms.DeliveryMode;
import javax.jms.Session;
import javax.jms.Topic;
import javax.jms.Queue;
import javax.jms.MessageListener;
import javax.jms.Message;

import java.io.BufferedReader;
import java.io.InputStreamReader;

import java.util.StringTokenizer;
import java.util.NoSuchElementException;
import java.util.Properties;
import java.util.Vector;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

//This is a very simple parser which uses the JMS server : it can dynamically use JMS objects
//
//Commands:
//- object.method(param[,...]) 
//  where param is an object, a string, a boolean or an integer 
//	invoke the selected method on the object
//  ex: session.createTopic("myTopic") 
//      connection.createTopicSession(false,1);
//
//- result=object.method(param[,...])
//  store the result of the method in the variable 'result'
//  ex: ID=connection.getClientID()
//
//- Some macros exist : 
//  pub(name of the topic,text of the msg) //send a TextMessage to a topic
//  newSubscriber=sub(name of the topic) //create a new TopicSubscriber for this topic
//  subscriber.listen() //create a listener for this MessageConsumer
//
//- When TestClient starts, it creates one connection (c), and one session (s)

public class TestClient
{		
	static Properties prop;
	static boolean assign;
	static String where;
	static int val;
	static Object obj;
	static TopicPublisher publisherTopic;
	static TopicSession sessionTopic;
	static QueueSender publisherQueue;
	static QueueSession sessionQueue;
        static InitialContext initialContext;

	//if the command is a macro ( obj.macro() or macro() ), expand it
	static boolean macro(String str,Object[] objects,Object[] names)	throws Exception
	{
		
		if (str.equals("pubq")) {
			Queue dest = (Queue)initialContext.lookup("queue/"+(String)objects[0]);
			TextMessage content=sessionQueue.createTextMessage();
			content.setText((String)objects[1]);
			publisherQueue.send(dest,content);
			return true;
		}
		
		if (str.equals("pub")) {
			Topic dest = (Topic)initialContext.lookup("topic/"+(String)objects[0]);
			TextMessage content=sessionTopic.createTextMessage();
			content.setText((String)objects[1]);
			publisherTopic.publish(dest,content);
			return true;
		}

		if (str.equals("pub2")) {
			Topic dest = (Topic)initialContext.lookup("topic/"+(String)objects[0]);
			BytesMessage bytesmsg1 = sessionTopic.createBytesMessage();
            byte[] data = new byte[100*1024];
            bytesmsg1.writeBytes (data, 0 , 10*1024);
			publisherTopic.publish(dest,bytesmsg1);
			return true;
		}

		if (str.equals("listen")) {
			TestListener listener=new TestListener(val++);
			((MessageConsumer)obj).setMessageListener(listener);
			return true;
		}
		
		if (str.equals("unlisten")) {			
			((MessageConsumer)obj).setMessageListener(null);
			return true;
		}

		if (str.equals("subq")) {
			Queue dest = (Queue)initialContext.lookup("queue/"+(String)objects[0]);
			QueueReceiver sub=sessionQueue.createReceiver(dest);
			prop.put(where,sub);
			return true;
		}
		
		if (str.equals("sub")) {
			Topic dest = (Topic)initialContext.lookup("topic/"+(String)objects[0]);
			TopicSubscriber sub=sessionTopic.createSubscriber(dest);
			prop.put(where,sub);
			return true;
		}

		return false;
	}

	static Method searchForMethod(Object obj,String methodName,Class[] classArray) throws NoSuchMethodException
	{
		Method method[]=obj.getClass().getMethods();
		
		for(int i=0;i<method.length;i++) {
			
			if (!method[i].getName().equals(methodName)) continue;
			Class[] act=method[i].getParameterTypes();
			
			if (act.length!=classArray.length) continue;
			
			for(int j=0;j<act.length;j++) 
				if (!act[j].isAssignableFrom(classArray[j])) continue;
			
			return method[i];
		}
		
		throw new NoSuchMethodException();
	}
	
	static void execute(String s) throws Exception
	{
		assign=false;
		where="";
		obj=null;
		String methodName=null;
				
		if (s.equals("q")) {						
			System.exit(0);
		}
		
		StringTokenizer st = new StringTokenizer(s," (),.\"=",true);
		String cmd=st.nextToken();
				
		String next=st.nextToken(); // . or = or (
		if (next.equals("="))
		{
			assign=true;
			where=cmd;
			cmd=st.nextToken();
			next=st.nextToken();
			if (!next.equals(".")) {
				if (next.equals("(")) {
					//this is a=Macro(...)
					methodName=cmd;
				} else throw new RuntimeException("Syntax error (.)");
			}
		}

		
		if (methodName==null)
			if (!next.equals("(")) {
				
				obj=prop.get(cmd);
				if (obj==null) throw new RuntimeException("Unknown object '"+cmd+"'");

				methodName=st.nextToken(); // this is the method name

				cmd=st.nextToken(); // (
				if (!cmd.equals("(")) throw new RuntimeException("Bad syntax");
				
			} else {
				//this is a macro call : Macro(...)
				methodName=cmd;
			}
			
				
		Vector objects=new Vector();
		Vector names=new Vector();
		Vector classes=new Vector();
				
		while (true) {
			cmd=st.nextToken();
					
			if (cmd.equals(" ")) continue;
			if (cmd.equals(",")) continue;
			if (cmd.equals(")")) break;
			
			if (cmd.equals("\"")) {
				
				String str="";
				cmd=st.nextToken();
				
				while (!cmd.equals("\"")) {
					str+=cmd;
					cmd=st.nextToken();
				}
							
				names.add(str);
				objects.add(str);
				classes.add(String.class);
				continue;
			}

			names.add(cmd);
			
			if (Character.isDigit(cmd.charAt(0))) {
				//an integer
				objects.add(new Integer(Integer.parseInt(cmd)));
				classes.add(Integer.TYPE);
			} else if (cmd.equals("true")) {
				//true
				objects.add(Boolean.TRUE);
				classes.add(Boolean.TYPE);
			} else if (cmd.equals("false")) {
				//false
				objects.add(Boolean.FALSE);
				classes.add(Boolean.TYPE);
			} else {
				//This is an object
				Object argument=prop.get(cmd);
				if (argument==null) throw new RuntimeException("Unknown object '"+cmd+"'");
				objects.add(argument);
				classes.add(argument.getClass());
			}
						
		}
		
		if (macro(methodName,objects.toArray(),names.toArray())) return;

		if (obj==null) throw new RuntimeException("You must provide an object name");		
		Class[] classArray=(Class[])classes.toArray(new Class[classes.size()]);		
		Method method=searchForMethod(obj,methodName,classArray);
		Object res=method.invoke(obj,objects.toArray());
				
		if (assign) {
			if (res==null) System.out.println("The return type is (void) !");
			else {
				System.out.println(s+" : "+where+" := "+res.toString());
				prop.put(where,res);			
			}
		} else if (res!=null) {
			System.out.println(s+" : "+res.toString());
		} else {
			System.out.println(s+" : (void)");
		}
				
	}
	
	public static void main(String[] args) throws Exception
	{		
		
		BufferedReader d = new BufferedReader(new InputStreamReader(System.in));
		prop=new Properties();

                initialContext = new InitialContext();

		TopicConnectionFactory factoryTopic = (TopicConnectionFactory)initialContext.lookup("ConnectionFactory");
		TopicConnection connectionTopic = factoryTopic.createTopicConnection("sahra","spot");
		sessionTopic = connectionTopic.createTopicSession(false,Session.AUTO_ACKNOWLEDGE);
		publisherTopic = sessionTopic.createPublisher(null);		
		publisherTopic.setDeliveryMode(DeliveryMode.PERSISTENT);
		
		prop.put("ct",connectionTopic);
		prop.put("st",sessionTopic);

		QueueConnectionFactory factoryQueue = (QueueConnectionFactory)initialContext.lookup("ConnectionFactory");
		QueueConnection connectionQueue = factoryQueue.createQueueConnection("sahra","spot");
		sessionQueue = connectionQueue.createQueueSession(false,Session.AUTO_ACKNOWLEDGE);
		publisherQueue = sessionQueue.createSender(null);		
		
		prop.put("cq",connectionQueue);
		prop.put("sq",sessionQueue);
		
		while (true) {
						
			try {
				execute(d.readLine());
				System.out.println("---"); 								
			} catch (NoSuchElementException e) {
				System.out.println("Bad syntax..."); 
			} catch (NoSuchMethodException e) {
				System.out.println("This method does not exist !"); 
			} catch (InvocationTargetException e) {				
				//This is not our fault !				
				e.printStackTrace();
			} catch (Exception e) { 
				System.out.println(e.getMessage()); 
				e.printStackTrace(); 
			}
			
		}
		
	}

	static class TestListener implements MessageListener
	{
		int num;
	
		TestListener(int n)
		{
			num=n;
		}
	
		public void onMessage(Message message)
		{
			System.out.println("MessageListener "+num+" : OnMessage("+message.toString()+")"); 
		}
	}
}

