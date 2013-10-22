//
//  SOSTestTransport.h
//  sec
//
//  Created by Michael Brouwer on 10/16/12.
//
//

#ifndef _SEC_SOSTESTTRANSPORT_H_
#define _SEC_SOSTESTTRANSPORT_H_

#include <SecureObjectSync/SOSTransport.h>

__BEGIN_DECLS

SOSTransportRef SOSTestTransportCreate(void);

CFDataRef SOSTestTransportDequeue(SOSTransportRef transport);

void SOSTestTransportDispose(SOSTransportRef transport);

__END_DECLS

#endif /* _SEC_SOSTESTTRANSPORT_H_ */
