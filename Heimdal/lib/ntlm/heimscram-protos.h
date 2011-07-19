/* This is a generated file */
#ifndef __heimscram_protos_h__
#define __heimscram_protos_h__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

int
heim_scram_client1 (
	const char */*username*/,
	heim_scram_data *ch,
	heim_scram_method,
	heim_scram **/*scram*/,
	heim_scram_data */*out*/);

int
heim_scram_client2 (
	heim_scram_data */*in*/,
	struct heim_scram_client *client,
	void *ctx,
	heim_scram */*scram*/,
	heim_scram_data */*out*/);

int
heim_scram_client3 (
	heim_scram_data */*in*/,
	heim_scram */*scram*/);

int
heim_scram_client_key (
	heim_scram_method /* method */,
	const char */*password*/,
	unsigned int iterations,
	heim_scram_data */*salt*/,
	heim_scram_data */*data*/);

void
heim_scram_free (heim_scram */*scram*/);

void
heim_scram_data_free (heim_scram_data */*data*/);

int
heim_scram_get_channel_binding (
	heim_scram */*scram*/,
	heim_scram_data */*ch*/);


int
heim_scram_server1 (
	heim_scram_data */*in*/,
	heim_scram_data *ch,
	heim_scram_method,
	struct heim_scram_server */*server*/,
	void */*ctx*/,
	heim_scram **/*scram*/,
	heim_scram_data */*out*/);

int
heim_scram_server2 (
	heim_scram_data */*in*/,
	heim_scram */*scram*/,
	heim_scram_data */*out*/);

int
heim_scram_stored_key(heim_scram_method method,
		      const char *password,
		      unsigned int iterations,
		      heim_scram_data *salt,
		      heim_scram_data *client_key,
		      heim_scram_data *stored_key,
		      heim_scram_data *server_key);


int
heim_scram_salted_key(heim_scram_method method,
		      const char *password,
		      unsigned int iterations,
		      heim_scram_data *salt,
		      heim_scram_data *data);

int
heim_scram_generate(heim_scram_method method,
		    const heim_scram_data *stored_key,
		    const heim_scram_data *server_key,
		    const heim_scram_data *c1,
		    const heim_scram_data *s1,
		    const heim_scram_data *c2noproof,
		    heim_scram_data *clientSig,
		    heim_scram_data *serverSig);

int
heim_scram_validate_client_signature(heim_scram_method method,
				     const heim_scram_data *stored_key,
				     const heim_scram_data *client_signature,
				     const heim_scram_data *proof,
				     heim_scram_data *clientKey);

int

heim_scram_session_key(heim_scram_method method,
		       const heim_scram_data *stored_key,
		       const heim_scram_data *client_key,
		       const heim_scram_data *c1,
		       const heim_scram_data *s1,
		       const heim_scram_data *c2noproof,
		       heim_scram_data *sessionKey);

int
heim_scram_get_session_key(heim_scram *scram,
			   heim_scram_data *sessionKey);



#ifdef __cplusplus
}
#endif

#endif /* __heimscram_protos_h__ */
