readme.txt		This file.

client.crt		Binary DER certificate signed by the private key of rootca.
client_crt.pem		PEM form of client.crt
client_key.pem		PEM form of the private key corresponding to client.crt
simple_client.tar.gz	Variations of client.crt in binary ~DER format.  
			Variations occur across the entire certificate.  
			00000001-00106160
			tar file size:   ~846M			
			tar.gz file size: ~10M
resigned_client.tar.gz	Variations of client.crt in binary ~DER format.  
			Variations occur across the toBeSigned part of the certificate.  
			The signature block has been recreated.  
			00000000-00099981
			tar file size: 	 ~788M		
			tar.gz file size: ~22M

server.crt		Binary DER certificate signed by the private key of rootca.
server_crt.pem		PEM form of server.crt
server_key.pem		PEM form of the private key corresponding to server.crt
simple_server.tar.gz	Variations of server.crt in binary ~DER format.  
			Variations occur across the entire certificate.  
			00000001-00106167
			tar file size:   ~844M
			tar.gz file size: ~10M
resigned_server.tar.gz	Variations of server.crt in binary ~DER format.  
			Variations occur across the toBeSigned part of the certificate.  
			The signature block has been recreated.  
			00000000-00100068
			tar file size: 	 ~789M		
			tar.gz file size: ~22M

rootca.crt		Binary Self-signed DER certificate.
rootca_crt.pem		PEM form of rootca.crt
rootca_key.pem		PEM form of the private key corresponding to rootca.crt
simple_rootca.tar.gz	Variations of rootca.crt in binary ~DER format.  
			Variations occur across the entire certificate.  
			00000001-00106190
			tar file size:   ~862M
			tar.gz file size: ~10M
resigned_rootca.tar.gz	Variations of rootca.crt in binary ~DER format.  
			Variations occur across the toBeSigned part of the certificate.  
			The signature block has been recreated.  
			00000000-00099959
			tar file size: 	 ~806M
			tar.gz file size: ~22M
