package org.jboss.test.bench.servlet;

import java.util.ArrayList;
import java.util.Hashtable;

public class ConfigData {
	ArrayList names = new ArrayList();
	Hashtable infos = new Hashtable();

	public ConfigData() {
		setInfo("Hardware", "");
		setInfo("CPU", "");
		setInfo("RAM", "");
		setInfo("OS", "");
		setInfo("JDK Vendor/Version", "");
		setInfo("EJB Server", "");
		setInfo("Servlet Engine", "");
		setInfo("Web Server", "");
		setInfo("DB", "");
	}

	public int size() {
		return infos.size();
	}

	public String getName(int i) {
		return (String)names.get(i);
	}

	public String getValue(int i) {
		return (String)infos.get(names.get(i));
	}

	public void setInfo(String name, String value) {
		if (!infos.containsKey(name)) names.add(name);
		infos.put(name, value);
	}
}

