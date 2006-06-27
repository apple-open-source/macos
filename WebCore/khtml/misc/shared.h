#ifndef SHARED_H
#define SHARED_H

namespace khtml {

template<class type> class Shared
{
public:
    Shared() { _ref=0; /*counter++;*/ }
    ~Shared() { /*counter--;*/ }

    void ref() { _ref++;  }
    void deref() { 
	if(_ref) _ref--; 
	if(!_ref)
	    delete static_cast<type *>(this); 
    }
    bool hasOneRef() { //kdDebug(300) << "ref=" << _ref << endl;
    	return _ref==1; }

    int refCount() const { return _ref; }
//    static int counter;
protected:
    unsigned int _ref;

private:
    Shared(const Shared &);
    Shared &operator=(const Shared &);
};

template<class type> class TreeShared
{
public:
    TreeShared() { _ref = 0; m_parent = 0; /*counter++;*/ }
    TreeShared( type *parent ) { _ref=0; m_parent = parent; /*counter++;*/ }
    virtual ~TreeShared() { /*counter--;*/ }

    virtual void removedLastRef() { delete static_cast<type *>(this); }
    void ref() { _ref++;  }
    void deref() { 
	if(_ref) _ref--; 
	if(!_ref && !m_parent)
	    removedLastRef();
    }
    bool hasOneRef() { //kdDebug(300) << "ref=" << _ref << endl;
    	return _ref==1; }

    int refCount() const { return _ref; }
//    static int counter;

    void setParent(type *parent) { m_parent = parent; }
    type *parent() const { return m_parent; }
private:
    unsigned int _ref;
protected:
    type *m_parent;

private:
    TreeShared(const TreeShared &);
    TreeShared &operator=(const TreeShared &);
};

template <class T> class SharedPtr
{
public:
    SharedPtr() : m_ptr(0) {}
    SharedPtr(T *ptr) : m_ptr(ptr) { if (m_ptr) m_ptr->ref(); }
    SharedPtr(const SharedPtr &o) : m_ptr(o.m_ptr) { if (m_ptr) m_ptr->ref(); }
    ~SharedPtr() { if (m_ptr) m_ptr->deref(); }

    template <class U> SharedPtr(const SharedPtr<U> &o) : m_ptr(o.get()) { if (T *ptr = m_ptr) ptr->ref(); }

    bool isNull() const { return m_ptr == 0; }
    bool notNull() const { return m_ptr != 0; }

    void reset() { if (m_ptr) m_ptr->deref(); m_ptr = 0; }
    void reset(T *o) { if (o) o->ref(); if (m_ptr) m_ptr->deref(); m_ptr = o; }
    
    T * get() const { return m_ptr; }
    T &operator*() const { return *m_ptr; }
    T *operator->() const { return m_ptr; }

    bool operator!() const { return m_ptr == 0; }
    operator bool() const { return m_ptr != NULL; }

    inline friend bool operator==(const SharedPtr &a, const SharedPtr &b) { return a.m_ptr == b.m_ptr; }
    inline friend bool operator==(const SharedPtr &a, const T *b) { return a.m_ptr == b; }
    inline friend bool operator==(const T *a, const SharedPtr &b) { return a == b.m_ptr; }

    SharedPtr &operator=(const SharedPtr &);
    SharedPtr &operator=(T *);

private:
    T* m_ptr;

    operator int() const; // deliberately not implemented; helps prevent operator bool from converting to int accidentally
};

template <class T> SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<T> &o) 
{
    T *optr = o.m_ptr;
    if (optr)
        optr->ref();
    if (T *ptr = m_ptr)
        ptr->deref();
    m_ptr = optr;
    return *this;
}

template <class T> inline SharedPtr<T> &SharedPtr<T>::operator=(T *optr)
{
    if (optr)
        optr->ref();
    if (T *ptr = m_ptr)
        ptr->deref();
    m_ptr = optr;
    return *this;
}

template <class T> inline bool operator!=(const SharedPtr<T> &a, const SharedPtr<T> &b) { return !(a==b); }
template <class T> inline bool operator!=(const SharedPtr<T> &a, const T *b) { return !(a == b); }
template <class T> inline bool operator!=(const T *a, const SharedPtr<T> &b) { return !(a == b); }

};

#endif
