#ifndef __HOST_VNODE_H__
#define __HOST_VNODE_H__

#import "AMVnode.h"

@class String;

@interface HostVnode : Vnode
{
}

- (HostVnode *)vnodeForHost:(String *)name;

@end

#endif __HOST_VNODE_H__
