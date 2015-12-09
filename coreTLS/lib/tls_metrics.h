//
//  tls_metrics.h
//  coretls
//

#ifndef _TLS_METRICS_H_
#define _TLS_METRICS_H_ 1

#include <tls_handshake.h>

void tls_metric_client_finished(tls_handshake_t hdsk);
void tls_metric_destroyed(tls_handshake_t hdsk);

#endif /* _TLS_METRICS_H_ */
