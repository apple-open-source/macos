package org.jboss.test.cmp2.readonly;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

public abstract class PublisherBean implements EntityBean {
	transient private EntityContext ctx;

    public PublisherBean() {}

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

	public abstract Collection getBooks();
	public abstract void setBooks(Collection books);

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
