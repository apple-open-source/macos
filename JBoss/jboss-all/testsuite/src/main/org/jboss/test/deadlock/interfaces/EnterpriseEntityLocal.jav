package org.jboss.test.deadlock.interfaces;


import javax.ejb.*;

public interface EnterpriseEntityLocal extends EJBLocalObject
{

  public String callBusinessMethodA();
  public String callBusinessMethodB();
  public String callBusinessMethodB(String words);
  public String getName();
  public void setOtherField(int value);
  public int getOtherField();
  public void setNext(EnterpriseEntityLocal next);
  public EnterpriseEntityLocal getNext();
  public void callAnotherBean(BeanOrder beanOrder);
  public EnterpriseEntity createEntity(String newName);
}
