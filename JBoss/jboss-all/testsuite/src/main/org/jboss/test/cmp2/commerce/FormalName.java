package org.jboss.test.cmp2.commerce;


public class FormalName implements java.io.Serializable {
	private String first;
	private char mi;
	private String last;
	
	public FormalName() {
	}
	
	public FormalName(String first, char mi, String last) {
		setFirst(first);
		setMi(mi);
		setLast(last);
	}
	
	public String getFirst() {
		return first;
	}
	
	public void setFirst(String first) {
      if(first == null) {
         throw new IllegalArgumentException("First is null");
      }
      first = first.trim();
      if(first.length() == 0) {
         throw new IllegalArgumentException("First is zero length");
      }
		this.first = first;
	}

	public char getMi() {
		return mi;
	}
	
	public void setMi(char mi) {
		this.mi = mi;
	}
	public String getLast() {
		return last;
	}
	
	public void setLast(String last) {
      if(last == null) {
         throw new IllegalArgumentException("Last is null");
      }
      last = last.trim();
      if(last.length() == 0) {
         throw new IllegalArgumentException("Last is zero length");
      }
		this.last = last;
	}

	public boolean equals(Object obj) {
		if(obj instanceof FormalName) {
			FormalName name = (FormalName)obj;
			return equal(name.first, first) && 
               name.mi==mi && 
               equal(name.last, last); 
		}
		return false;
	}
	
	private boolean equal(String a, String b) {
		return (a==null && b==null) || (a!=null && a.equals(b));
	}
	
	public String toString() {
		StringBuffer buf = new StringBuffer();
		if(first != null) {
			buf.append(first);
		}
		if(mi != '\u0000') {
			if(first != null) {
				buf.append(" ");
			}
			buf.append(mi).append(".");
		}
		if(last != null) {
			if(first != null || mi != '\u0000') {
				buf.append(" ");
			}
			buf.append(last);
		}
		return buf.toString();
	}
}
