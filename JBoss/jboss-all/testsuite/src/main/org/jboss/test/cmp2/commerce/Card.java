package org.jboss.test.cmp2.commerce;


public class Card implements java.io.Serializable {
   public static final int VISA = 0;
   public static final int AMERICAN_EXPRESS = 1;
   public static final int MASTER_CARD = 2;
   public static final int DISCOVER = 3;
   
   private int type;
	private FormalName cardHolder;
	private String cardNumber;
	private int billingZip;
   
   public Card() {
	}
	
	public FormalName getCardHolder() {
		return cardHolder;
	}
	
	public void setCardHolder(FormalName name) {
		this.cardHolder = name;
	}
	
	public int getBillingZip() {
		return billingZip;
	}
	
	public void setBillingZip(int zip) {
		this.billingZip = zip;
	}
	
	public String getCardNumber() {
		return cardNumber;
	}
	
	public void setCardNumber(String num) {
		this.cardNumber = num;
	}
	
   public int getType() {
      return type;
   }
   
   public void setType(int type) {
      if(type != VISA &&
            type != AMERICAN_EXPRESS &&
            type != MASTER_CARD &&
            type != DISCOVER) {
         throw new IllegalArgumentException("Unknown card type: "+type);
      }
      this.type = type;
   }

	public boolean equals(Object obj) {
		if(obj instanceof Card) {
			Card c = (Card)obj;
			return 
					equal(c.cardNumber, cardNumber) && 
					equal(c.cardHolder, cardHolder) &&
					c.type == type &&
					c.billingZip == billingZip;
		}
		return false;
	}
	
	private boolean equal(Object a, Object b) {
		return (a==null && b==null) || (a!=null && a.equals(b));
	}
	
	public String toString() {
		return cardNumber;
	}		
}
