To install a new version of IOFireWireFamily:
1) As root, run 'make installhdrs' in the IOFireWireFamily directory.
This will install the headers into the Kernel framework
2) run 'make' in the IOFireWireFamily directory.
3) As root, copy IOFireWireFamily.kext from the IOFireWireFamily directory to /System/Library/Extensions

If you're doing development work in IOFireWireFamily, a useful alternative is to create a soft link from
/System/Library/Extensions/IOFireWireFamily.kext/IOFireWireFamily to
(sandbox)/IOFireWireFamily/IOFireWireFamily.kext/IOFireWireFamily.
Then every time you rebuild the project the new code wil be ready to be loaded.
