package org.jboss.test.cmp2.readonly;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

public abstract class BookBean implements EntityBean {
	transient private EntityContext ctx;

    public BookBean() {}

	public Integer ejbCreate(Integer id) throws CreateException {
		setId(id);
		return null;
	}

	public void ejbPostCreate(Integer id) {
	}

	public abstract Integer getId();
	public abstract void setId(Integer id);

	public abstract String getName();
	public abstract void setName(String name);

	public abstract String getIsbn();
	public abstract void setIsbn(String isbn);

	public abstract Publisher getPublisher();
	public abstract void setPublisher(Publisher publisher);

	public void setEntityContext(EntityContext ctx) {
		this.ctx = ctx;
	}

	public void unsetEntityContext() {
		this.ctx = null;
	}

	public void ejbActivate() {
	}

	public void ejbPassivate() {
	}

	public void ejbLoad() {
	}

	public void ejbStore() {
	}

	public void ejbRemove() {
	}
}
