//
// SuperBlob - a typed bag of Blobs
//
#ifndef _H_SUPERBLOB
#define _H_SUPERBLOB

#include <security_utilities/blob.h>
#include <map>

namespace Security {


//
// A SuperBlob is a Blob that contains multiple sub-Blobs of varying type.
// The SuperBlob is contiguous and contains a directory of its sub-blobs.
// A Maker is included.
//
// SuperBlobCore lets you define your own SuperBlob type. To just use a generic
// SuperBlob, use SuperBlob<> below.
//
template <class _BlobType, uint32_t _magic, class _Type>
class SuperBlobCore: public Blob<_BlobType, _magic> {
public:
	class Maker; friend class Maker;
	
	typedef _Type Type;
	
	// echoes from parent BlobCore (the C++ type system is too restrictive here)
	typedef BlobCore::Offset Offset;
	template <class BlobType> BlobType *at(Offset offset) { return BlobCore::at<BlobType>(offset); }
	template <class BlobType> const BlobType *at(Offset offset) const { return BlobCore::at<BlobType>(offset); }
	
	void setup(size_t size, unsigned count)
	{ this->initialize(size); this->mCount = count; }

	struct Index {
		Endian<Type> type;			// type of sub-Blob
		Endian<Offset> offset;		// starting offset
	};
	
	bool validateBlob(size_t maxSize = 0) const;
	
	unsigned count() const { return mCount; }

	// access by index number
	Type type(unsigned n) const { assert(n < mCount); return mIndex[n].type; }
	const BlobCore *blob(unsigned n) const
		{ assert(n < mCount); return mIndex[n].offset ? at<const BlobCore>(mIndex[n].offset) : NULL; }
	template <class BlobType>
	const BlobType *blob(unsigned n) const { return BlobType::specific(blob(n)); }

	// access by index type (assumes unique types)
	const BlobCore *find(Type type) const;
	template <class BlobType>
	const BlobType *find(Type type) const { return BlobType::specific(find(type)); }
	
private:
	Endian<uint32_t> mCount;		// number of sub-Blobs following
	Index mIndex[0];				// <count> IndexSlot structures
	// followed by sub-Blobs, packed and ordered in an undefined way
};


template <class _BlobType, uint32_t _magic, class _Type>
inline bool SuperBlobCore<_BlobType, _magic, _Type>::validateBlob(size_t maxSize /* = 0 */) const
{
	unsigned count = mCount;
	size_t ixLimit = sizeof(SuperBlobCore) + count * sizeof(Index);	// end of index vector
	if (!BlobCore::validateBlob(_magic, ixLimit, maxSize))
		return false;
	for (const Index *ix = mIndex + count - 1; ix >= mIndex; ix--) {
		Offset offset = ix->offset;
		if (offset)																		// if non-null
		if (offset < ixLimit														// offset not too small
			|| offset + sizeof(BlobCore) > this->length()							// fits Blob header (including length field)
			|| offset + at<const BlobCore>(offset)->length() > this->length())		// fits entire blob
			return false;
	}
	return true;
}


//
// A generic SuperBlob ready for use. You still need to specify a magic number.
//
template <uint32_t _magic, class _Type = uint32_t>
class SuperBlob : public SuperBlobCore<SuperBlob<_magic, _Type>, _magic, _Type> {
};


template <class _BlobType, uint32_t _magic, class _Type>
const BlobCore *SuperBlobCore<_BlobType, _magic, _Type>::find(Type type) const
{
	for (unsigned slot = 0; slot < mCount; slot++)
		if (mIndex[slot].type == type)
			return mIndex[slot].offset ? at<const BlobCore>(mIndex[slot].offset) : NULL;
	return NULL;	// not found
}


//
// A SuperBlob::Maker simply assembles multiple Blobs into a single, indexed
// super-blob. Just add() sub-Blobs by type and call make() to get
// the result, malloc'ed. A Maker is not resettable.
// Maker can repeatedly make SuperBlobs from the same (cached) inputs.
// It can also tell you how big its output will be, given established contents
// plus (optional) additional sizes of blobs yet to come.
//
template <class _BlobType, uint32_t _magic, class _Type>
class SuperBlobCore<_BlobType, _magic, _Type>::Maker {
public:
	Maker() { }
	
	Maker(const Maker &src)
	{
		for (typename BlobMap::iterator it = mPieces.begin(); it != mPieces.end(); ++it)
			mPieces.insert(make_pair(it->first, it->second->clone()));
	}

	~Maker()
	{
		for (typename BlobMap::iterator it = mPieces.begin(); it != mPieces.end(); ++it)
			::free(it->second);
	}
	
	void add(Type type, BlobCore *blob);		// takes ownership of blob
	void add(const _BlobType *blobs);			// copies all blobs
	void add(const Maker &maker);				// ditto
	
	bool contains(Type type) const				// see if we have this type already
		{ return mPieces.find(type) != mPieces.end(); }
	BlobCore *get(Type type) const
		{
			typename BlobMap::const_iterator it = mPieces.find(type);
			return (it == mPieces.end()) ? NULL : it->second;
		}
	
	size_t size(size_t size1 = 0, ...) const;	// size with optional additional blob sizes
	_BlobType *make() const;					// create (malloc) and return SuperBlob
	_BlobType *operator () () const { return make(); }

private:
	typedef std::map<Type, BlobCore *> BlobMap;
	BlobMap mPieces;
};


//
// Add a Blob to a SuperBlob::Maker.
// This takes ownership of the blob, which must have been malloc'ed.
// Any previous value set for this Type will be freed immediately.
//
template <class _BlobType, uint32_t _magic, class _Type>
void SuperBlobCore<_BlobType, _magic, _Type>::Maker::add(Type type, BlobCore *blob)
{
	pair<typename BlobMap::iterator, bool> r = mPieces.insert(make_pair(type, blob));
	if (!r.second) {	// already there
		secdebug("superblob", "Maker %p replaces type=%d", this, type);
		::free(r.first->second);
		r.first->second = blob;
	}
}

template <class _BlobType, uint32_t _magic, class _Type>
void SuperBlobCore<_BlobType, _magic, _Type>::Maker::add(const _BlobType *blobs)
{
	for (uint32_t ix = 0; ix < blobs->mCount; ix++)
		this->add(blobs->mIndex[ix].type, blobs->blob(ix)->clone());
}

template <class _BlobType, uint32_t _magic, class _Type>
void SuperBlobCore<_BlobType, _magic, _Type>::Maker::add(const Maker &maker)
{
	for (typename BlobMap::const_iterator it = maker.mPieces.begin(); it != maker.mPieces.end(); ++it)
		this->add(it->first, it->second->clone());
}


//
// Calculate the size the new SuperBlob would have, given the contents of the Maker
// so far, plus additional blobs with the sizes given.
//
template <class _BlobType, uint32_t _magic, class _Type>
size_t SuperBlobCore<_BlobType, _magic, _Type>::Maker::size(size_t size1, ...) const
{
	// count established blobs
	unsigned count = mPieces.size();
	size_t total = 0;
	for (typename BlobMap::const_iterator it = mPieces.begin(); it != mPieces.end(); ++it)
		total += it->second->length();

	// add preview blob sizes to calculation (if any)
	if (size1) {
		va_list args;
		va_start(args, size1);
		do {
			count++;
			total += size1;
			size1 = va_arg(args, size_t);
		} while (size1);
		va_end(args);
	}

	return sizeof(SuperBlobCore) + count * sizeof(Index) + total;
}


//
// Finish SuperBlob construction and return the new, malloc'ed, SuperBlob.
// This can be done repeatedly.
//
template <class _BlobType, uint32_t _magic, class _Type>
_BlobType *SuperBlobCore<_BlobType, _magic, _Type>::Maker::make() const
{
	Offset pc = sizeof(SuperBlobCore) + mPieces.size() * sizeof(Index);
	Offset total = size();
	_BlobType *result = (_BlobType *)malloc(total);
	if (!result)
		UnixError::throwMe(ENOMEM);
	result->setup(total, mPieces.size());
	unsigned n = 0;
	for (typename BlobMap::const_iterator it = mPieces.begin(); it != mPieces.end(); ++it) {
		result->mIndex[n].type = it->first;
		result->mIndex[n].offset = pc;
		memcpy(result->at<unsigned char>(pc), it->second, it->second->length());
		pc += it->second->length();
		n++;
	}
	secdebug("superblob", "Maker %p assembles %ld blob(s) into %p (size=%d)",
		this, mPieces.size(), result, total);
	return result;
}


}	// Security

#endif //_H_SUPERBLOB
