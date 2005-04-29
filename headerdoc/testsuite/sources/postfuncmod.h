/*! @function getParentIterator
    @abstract Returns an iterator over an registry entry's parent entries in a specified plane.
    @param plane The plane object.
    @result Returns an iterator over the parents of the registry entry, or zero if there is a resource failure. The iterator must be released when the iteration is finished. All objects returned by the iteration are retained while the iterator is valid, though they may no longer be attached during the iteration. */

    virtual OSIterator * getParentIterator( const IORegistryPlane * plane )
									const;
    virtual void applyToParents( IORegistryEntryApplierFunction applier,
                                 void * context,
                                 const IORegistryPlane * plane ) const;

/*! @function getParentEntry
    @abstract Returns an registry entry's first parent entry in a plane.
    @discussion This function will return the parent to which a registry entry was first attached. Since the majority of registry entrys have only one provider, this is a useful simplification.
    @param plane The plane object.
    @result Returns the first parent of the registry entry, or zero if the entry is not attached into the registry in that plane. The parent is retained while the entry is attached, and should not be released by the caller. */

    virtual IORegistryEntry * getParentEntry( const IORegistryPlane * plane ) const;
