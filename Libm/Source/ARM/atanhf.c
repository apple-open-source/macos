/*
 *  atanhf.c
 *  cLibm
 *
 *  Created by Stephen Canon on 7/9/07.
 *  Copyright 2007 Apple Inc. All rights reserved.
 *
 *	This file implements the C99 function atanhf with error < .503 ulps.
 *
 */

#include "math.h"
#include <stdint.h>

/*******************************************************
 Table generation code:
 
 #include <math.h>
 #include <stdio.h>
 
 void gen_table() {
	int i,p,q;
	for (i=0; i<256; ++i) {
		double a = (double)i / 256.0;
		printf("{ %a,", atanh(a));
		printf(" %a,", 0.5*log1p(a));
		printf(" %a,", 1.0 / (1.0 + a));
		printf(" %a },", 1.0 / (1.0 - a*a));
 
		if (!i) printf("\t// a = 0\n");
		else {
			p = i; q = 256;
			while (!(p % 2)) {
				p = p/2; q = q/2;
			}
			printf("\t// a = %d/%d\n", p, q);
		}
	}
 }
 
 int main(int argc, char *argv[]) {
	gen_table();
	return 0;
 }
 
*********************************************************/

struct atanhf_table_entry {
	double atanh_hi;
	double half_log1p_hi;
	double one_plus_hi_recip;
	double one_minus_hisquared_recip;
};

struct atanhf_table_entry atanhf_table[] = {
	{ 0x0p+0, 0x0p+0, 0x1p+0, 0x1p+0 },	// a = 0
	{ 0x1.000055558888bp-8, 0x1.ff00aa2b10bcp-10, 0x1.fe01fe01fe02p-1, 0x1.000100010001p+0 },	// a = 1/256
	{ 0x1.000155588891bp-7, 0x1.fe02a6b106789p-9, 0x1.fc07f01fc07fp-1, 0x1.00040010004p+0 },	// a = 1/128
	{ 0x1.800480184d69p-7, 0x1.7dc475f810a77p-8, 0x1.fa11caa01fa12p-1, 0x1.0009005102d92p+0 },	// a = 3/256
	{ 0x1.000555888ad1cp-6, 0x1.fc0a8b0fc03e4p-8, 0x1.f81f81f81f82p-1, 0x1.001001001001p+0 },	// a = 1/64
	{ 0x1.400a6b46f591bp-6, 0x1.3cea44346a575p-7, 0x1.f6310aca0dbb5p-1, 0x1.001902713d0efp+0 },	// a = 5/256
	{ 0x1.80120184f3dedp-6, 0x1.7b91b07d5b11bp-7, 0x1.f44659e4a4271p-1, 0x1.00240510b659ap+0 },	// a = 3/128
	{ 0x1.c01c989e21e45p-6, 0x1.b9fc027af9198p-7, 0x1.f25f644230ab5p-1, 0x1.00310962cbe9p+0 },	// a = 7/256
	{ 0x1.001558891aee2p-5, 0x1.f829b0e7833p-7, 0x1.f07c1f07c1f08p-1, 0x1.0040100401004p+0 },	// a = 1/32
	{ 0x1.201e65c5878dfp-5, 0x1.1b0d98923d98p-6, 0x1.ee9c7f8458e02p-1, 0x1.005119a91e82ap+0 },	// a = 9/256
	{ 0x1.4029b471650a5p-5, 0x1.39e87b9febd6p-6, 0x1.ecc07b301eccp-1, 0x1.0064271f48383p+0 },	// a = 5/128
	{ 0x1.60378514ed016p-5, 0x1.58a5bafc8e4d5p-6, 0x1.eae807aba01ebp-1, 0x1.0079394c14f5fp+0 },	// a = 11/256
	{ 0x1.804818569482p-5, 0x1.77458f632dcfcp-6, 0x1.e9131abf0b767p-1, 0x1.0090512da9af7p+0 },	// a = 3/64
	{ 0x1.a05baefe1fa74p-5, 0x1.95c830ec8e3ebp-6, 0x1.e741aa59750e4p-1, 0x1.00a96fdad7784p+0 },	// a = 13/256
	{ 0x1.c07289f7b9245p-5, 0x1.b42dd711971bfp-6, 0x1.e573ac901e574p-1, 0x1.00c496833c7a5p+0 },	// a = 7/128
	{ 0x1.e08cea570e1e7p-5, 0x1.d276b8adb0b52p-6, 0x1.e3a9179dc1a73p-1, 0x1.00e1c66f67ea5p+0 },	// a = 15/256
	{ 0x1.005588ad375adp-4, 0x1.f0a30c01162a6p-6, 0x1.e1e1e1e1e1e1ep-1, 0x1.010101010101p+0 },	// a = 1/16
	{ 0x1.1066a036f9cecp-4, 0x1.075983598e471p-5, 0x1.e01e01e01e01ep-1, 0x1.012247b2f1021p+0 },	// a = 17/256
	{ 0x1.2079dc9754942p-4, 0x1.16536eea37ae1p-5, 0x1.de5d6e3f8868ap-1, 0x1.01459c19905abp+0 },	// a = 9/128
	{ 0x1.308f5eb6e013ep-4, 0x1.253f62f0a1417p-5, 0x1.dca01dca01dcap-1, 0x1.016affe2d6e1p+0 },	// a = 19/256
	{ 0x1.40a74799e283ep-4, 0x1.341d7961bd1d1p-5, 0x1.dae6076b981dbp-1, 0x1.019274d68f3fdp+0 },	// a = 5/64
	{ 0x1.50c1b861eec5ep-4, 0x1.42edcbea646fp-5, 0x1.d92f2231e7f8ap-1, 0x1.01bbfcd68d99fp+0 },	// a = 21/256
	{ 0x1.60ded24f86c9p-4, 0x1.51b073f06183fp-5, 0x1.d77b654b82c34p-1, 0x1.01e799dee9716p+0 },	// a = 11/128
	{ 0x1.70feb6c3c1a5ap-4, 0x1.60658a93750c4p-5, 0x1.d5cac807572b2p-1, 0x1.02154e063adfbp+0 },	// a = 23/256
	{ 0x1.81218741f5a6ap-4, 0x1.6f0d28ae56b4cp-5, 0x1.d41d41d41d41dp-1, 0x1.02451b7ddb2d2p+0 },	// a = 3/32
	{ 0x1.9147657166782p-4, 0x1.7da766d7b12ccp-5, 0x1.d272ca3fc5b1ap-1, 0x1.0277049228d5bp+0 },	// a = 25/256
	{ 0x1.a170731ef7b1ep-4, 0x1.8c345d6319b21p-5, 0x1.d0cb58f6ec074p-1, 0x1.02ab0baacf0acp+0 },	// a = 13/128
	{ 0x1.b19cd23ee3f68p-4, 0x1.9ab42462033adp-5, 0x1.cf26e5c44bfc6p-1, 0x1.02e1334b10c2cp+0 },	// a = 27/256
	{ 0x1.c1cca4ee78e02p-4, 0x1.a926d3a4ad563p-5, 0x1.cd85689039b0bp-1, 0x1.03197e121767bp+0 },	// a = 7/64
	{ 0x1.d2000d75d7f6p-4, 0x1.b78c82bb0eda1p-5, 0x1.cbe6d9601cbe7p-1, 0x1.0353eebb45366p+0 },	// a = 29/256
	{ 0x1.e2372e49bce55p-4, 0x1.c5e548f5bc743p-5, 0x1.ca4b3055ee191p-1, 0x1.0390881e8b62p+0 },	// a = 15/128
	{ 0x1.f2722a0d493b5p-4, 0x1.d4313d66cb35dp-5, 0x1.c8b265afb8a42p-1, 0x1.03cf4d30c41p+0 },	// a = 31/256
	{ 0x1.015891c9eaef7p-3, 0x1.e27076e2af2e6p-5, 0x1.c71c71c71c71cp-1, 0x1.041041041041p+0 },	// a = 1/8
	{ 0x1.097a1ef16543fp-3, 0x1.f0a30c01162a6p-5, 0x1.c5894d10d4986p-1, 0x1.045366c839bdap+0 },	// a = 33/256
	{ 0x1.119dce19bdbafp-3, 0x1.fec9131dbeabbp-5, 0x1.c3f8f01c3f8fp-1, 0x1.0498c1cb191d7p+0 },	// a = 17/128
	{ 0x1.19c3b0fa86d54p-3, 0x1.0671512ca596ep-4, 0x1.c26b5392ea01cp-1, 0x1.04e055790001p+0 },	// a = 35/256
	{ 0x1.21ebd96730f38p-3, 0x1.0d77e7cd08e59p-4, 0x1.c0e070381c0ep-1, 0x1.052a255d27987p+0 },	// a = 9/64
	{ 0x1.2a165950035bcp-3, 0x1.14785846742acp-4, 0x1.bf583ee868d8bp-1, 0x1.0576352223903p+0 },	// a = 37/256
	{ 0x1.324342c318e81p-3, 0x1.1b72ad52f67ap-4, 0x1.bdd2b899406f7p-1, 0x1.05c488925980ep+0 },	// a = 19/128
	{ 0x1.3a72a7ed6082cp-3, 0x1.2266f190a5accp-4, 0x1.bc4fd65883e7bp-1, 0x1.061523987cfeap+0 },	// a = 39/256
	{ 0x1.42a49b1ba196ap-3, 0x1.29552f81ff523p-4, 0x1.bacf914c1badp-1, 0x1.06680a4010668p+0 },	// a = 5/32
	{ 0x1.4ad92ebb84987p-3, 0x1.303d718e47fd3p-4, 0x1.b951e2b18ff23p-1, 0x1.06bd40b5ea891p+0 },	// a = 41/256
	{ 0x1.5310755c9fd18p-3, 0x1.371fc201e8f74p-4, 0x1.b7d6c3dda338bp-1, 0x1.0714cb48c1542p+0 },	// a = 21/128
	{ 0x1.5b4a81b18894fp-3, 0x1.3dfc2b0ecc62ap-4, 0x1.b65e2e3beee05p-1, 0x1.076eae69b99dap+0 },	// a = 43/256
	{ 0x1.63876690e907p-3, 0x1.44d2b6ccb7d1ep-4, 0x1.b4e81b4e81b4fp-1, 0x1.07caeeacfc334p+0 },	// a = 11/64
	{ 0x1.6bc736f69aa39p-3, 0x1.4ba36f39a55e5p-4, 0x1.b37484ad806cep-1, 0x1.082990ca50557p+0 },	// a = 45/256
	{ 0x1.740a0604c5adbp-3, 0x1.526e5e3a1b438p-4, 0x1.b2036406c80d9p-1, 0x1.088a999dbbc4p+0 },	// a = 23/128
	{ 0x1.7c4fe70505b75p-3, 0x1.59338d9982086p-4, 0x1.b094b31d922a4p-1, 0x1.08ee0e282885bp+0 },	// a = 47/256
	{ 0x1.8498ed69936dcp-3, 0x1.5ff3070a793d4p-4, 0x1.af286bca1af28p-1, 0x1.0953f39010954p+0 },	// a = 3/16
	{ 0x1.8ce52cce73dc7p-3, 0x1.66acd4272ad51p-4, 0x1.adbe87f94905ep-1, 0x1.09bc4f222fa0bp+0 },	// a = 49/256
	{ 0x1.9534b8faad565p-3, 0x1.6d60fe719d21dp-4, 0x1.ac5701ac5701bp-1, 0x1.0a2726523b088p+0 },	// a = 25/128
	{ 0x1.9d87a5e18238bp-3, 0x1.740f8f54037a5p-4, 0x1.aaf1d2f87ebfdp-1, 0x1.0a947ebba04fdp+0 },	// a = 51/256
	{ 0x1.a5de07a3b1bc2p-3, 0x1.7ab890210d909p-4, 0x1.a98ef606a63bep-1, 0x1.0b045e224a2f9p+0 },	// a = 13/64
	{ 0x1.ae37f290bf096p-3, 0x1.815c0a14357ebp-4, 0x1.a82e65130e159p-1, 0x1.0b76ca736c81ap+0 },	// a = 53/256
	{ 0x1.b6957b283ec92p-3, 0x1.87fa06520c911p-4, 0x1.a6d01a6d01a6dp-1, 0x1.0bebc9c657399p+0 },	// a = 27/128
	{ 0x1.bef6b61b2b693p-3, 0x1.8e928de886d41p-4, 0x1.a574107688a4ap-1, 0x1.0c63625d50a6p+0 },	// a = 55/256
	{ 0x1.c75bb84d40518p-3, 0x1.9525a9cf456b4p-4, 0x1.a41a41a41a41ap-1, 0x1.0cdd9aa677344p+0 },	// a = 7/32
	{ 0x1.cfc496d65c453p-3, 0x1.9bb362e7dfb83p-4, 0x1.a2c2a87c51cap-1, 0x1.0d5a793caaf5cp+0 },	// a = 57/256
	{ 0x1.d8316703eb314p-3, 0x1.a23bc1fe2b563p-4, 0x1.a16d3f97a4b02p-1, 0x1.0dda04e87f26ep+0 },	// a = 29/128
	{ 0x1.e0a23e5a57a7p-3, 0x1.a8becfc882f19p-4, 0x1.a01a01a01a01ap-1, 0x1.0e5c44a133fbep+0 },	// a = 59/256
	{ 0x1.e917329684475p-3, 0x1.af3c94e80bff3p-4, 0x1.9ec8e951033d9p-1, 0x1.0ee13f8db8f93p+0 },	// a = 15/64
	{ 0x1.f19059af4d646p-3, 0x1.b5b519e8fb5a4p-4, 0x1.9d79f176b682dp-1, 0x1.0f68fd05b8216p+0 },	// a = 61/256
	{ 0x1.fa0dc9d7131ffp-3, 0x1.bc286742d8cd6p-4, 0x1.9c2d14ee4a102p-1, 0x1.0ff38492aa44bp+0 },	// a = 31/128
	{ 0x1.0147ccbea629ap-2, 0x1.c2968558c18c1p-4, 0x1.9ae24ea5510dap-1, 0x1.1080ddf0f4c2cp+0 },	// a = 63/256
	{ 0x1.058aefa811451p-2, 0x1.c8ff7c79a9a22p-4, 0x1.999999999999ap-1, 0x1.1111111111111p+0 },	// a = 1/4
	{ 0x1.09d0591f0bb21p-2, 0x1.cf6354e09c5dcp-4, 0x1.9852f0d8ec0ffp-1, 0x1.11a42618be5ddp+0 },	// a = 65/256
	{ 0x1.0e1814bbd9d56p-2, 0x1.d5c216b4fbb91p-4, 0x1.970e4f80cb872p-1, 0x1.123a25643da93p+0 },	// a = 33/128
	{ 0x1.12622e38a03abp-2, 0x1.dc1bca0abec7dp-4, 0x1.95cbb0be377aep-1, 0x1.12d3178798b4cp+0 },	// a = 67/256
	{ 0x1.16aeb1724557cp-2, 0x1.e27076e2af2e6p-4, 0x1.948b0fcd6e9ep-1, 0x1.136f054ff42a4p+0 },	// a = 17/64
	{ 0x1.1afdaa6958afbp-2, 0x1.e8c0252aa5a6p-4, 0x1.934c67f9b2ce6p-1, 0x1.140df7c4ed62dp+0 },	// a = 69/256
	{ 0x1.1f4f2542ff85bp-2, 0x1.ef0adcbdc5936p-4, 0x1.920fb49d0e229p-1, 0x1.14aff82a0438dp+0 },	// a = 35/128
	{ 0x1.23a32e49e74ecp-2, 0x1.f550a564b7b37p-4, 0x1.90d4f120190d5p-1, 0x1.1555100011555p+0 },	// a = 71/256
	{ 0x1.27f9d1ef3e177p-2, 0x1.fb9186d5e3e2bp-4, 0x1.8f9c18f9c18fap-1, 0x1.15fd4906c96f1p+0 },	// a = 9/32
	{ 0x1.2c531ccbb110cp-2, 0x1.00e6c45ad501dp-3, 0x1.8e6527af1373fp-1, 0x1.16a8ad3e4df4cp+0 },	// a = 73/256
	{ 0x1.30af1ba0717b8p-2, 0x1.0402594b4d041p-3, 0x1.8d3018d3018d3p-1, 0x1.175746e8cba4p+0 },	// a = 37/128
	{ 0x1.350ddb58402abp-2, 0x1.071b85fcd590dp-3, 0x1.8bfce8062ff3ap-1, 0x1.1809208c27917p+0 },	// a = 75/256
	{ 0x1.396f69087fd7cp-2, 0x1.0a324e27390e3p-3, 0x1.8acb90f6bf3aap-1, 0x1.18be44f3bb2f6p+0 },	// a = 19/64
	{ 0x1.3dd3d1f24e85fp-2, 0x1.0d46b579ab74bp-3, 0x1.899c0f601899cp-1, 0x1.1976bf321fe4ap+0 },	// a = 77/256
	{ 0x1.423b2383a6343p-2, 0x1.1058bf9ae4ad5p-3, 0x1.886e5f0abb04ap-1, 0x1.1a329aa30accap+0 },	// a = 39/128
	{ 0x1.46a56b58851f7p-2, 0x1.136870293a8bp-3, 0x1.87427bcc092b9p-1, 0x1.1af1e2ed3940cp+0 },	// a = 79/256
	{ 0x1.4b12b73c1dd95p-2, 0x1.1675cababa60ep-3, 0x1.8618618618618p-1, 0x1.1bb4a4046ed29p+0 },	// a = 5/16
	{ 0x1.4f83152a0f7b5p-2, 0x1.1980d2dd4236fp-3, 0x1.84f00c2780614p-1, 0x1.1c7aea2b8565dp+0 },	// a = 81/256
	{ 0x1.53f6934fa63f8p-2, 0x1.1c898c16999fbp-3, 0x1.83c977ab2beddp-1, 0x1.1d44c1f69021bp+0 },	// a = 41/128
	{ 0x1.586d400d24cbep-2, 0x1.1f8ff9e48a2f3p-3, 0x1.82a4a0182a4ap-1, 0x1.1e12384d11f8ap+0 },	// a = 83/256
	{ 0x1.5ce729f71680ap-2, 0x1.22941fbcf7966p-3, 0x1.8181818181818p-1, 0x1.1ee35a6c489p+0 },	// a = 21/64
	{ 0x1.61645fd7ab1bap-2, 0x1.2596010df763ap-3, 0x1.8060180601806p-1, 0x1.1fb835e98c5a2p+0 },	// a = 85/256
	{ 0x1.65e4f0b01c08ep-2, 0x1.2895a13de86a3p-3, 0x1.7f405fd017f4p-1, 0x1.2090d8b4c6bdcp+0 },	// a = 43/128
	{ 0x1.6a68ebba1bb84p-2, 0x1.2b9303ab89d25p-3, 0x1.7e225515a4f1dp-1, 0x1.216d511aff336p+0 },	// a = 87/256
	{ 0x1.6ef060694f581p-2, 0x1.2e8e2bae11d31p-3, 0x1.7d05f417d05f4p-1, 0x1.224dadc900489p+0 },	// a = 11/32
	{ 0x1.737b5e6cd3547p-2, 0x1.31871c9544185p-3, 0x1.7beb3922e017cp-1, 0x1.2331fdce15884p+0 },	// a = 89/256
	{ 0x1.7809f5b0cb028p-2, 0x1.347dd9a987d55p-3, 0x1.7ad2208e0ecc3p-1, 0x1.241a509ee3506p+0 },	// a = 45/128
	{ 0x1.7c9c365ffbdfcp-2, 0x1.3772662bfd85bp-3, 0x1.79baa6bb6398bp-1, 0x1.2506b61859accp+0 },	// a = 91/256
	{ 0x1.813230e574d5ap-2, 0x1.3a64c556945eap-3, 0x1.78a4c8178a4c8p-1, 0x1.25f73e82c35afp+0 },	// a = 23/64
	{ 0x1.85cbf5ee41f2ap-2, 0x1.3d54fa5c1f71p-3, 0x1.77908119ac60dp-1, 0x1.26ebfa94f2298p+0 },	// a = 93/256
	{ 0x1.8a69966b2d128p-2, 0x1.404308686a7e4p-3, 0x1.767dce434a9b1p-1, 0x1.27e4fb7789f5cp+0 },	// a = 47/128
	{ 0x1.8f0b23928bf15p-2, 0x1.432ef2a04e814p-3, 0x1.756cac201756dp-1, 0x1.28e252c86b994p+0 },	// a = 95/256
	{ 0x1.93b0aee21c2c8p-2, 0x1.4618bc21c5ec2p-3, 0x1.745d1745d1746p-1, 0x1.29e4129e4129ep+0 },	// a = 3/8
	{ 0x1.985a4a20edba2p-2, 0x1.49006804009d1p-3, 0x1.734f0c541fe8dp-1, 0x1.2aea4d8c2d024p+0 },	// a = 97/256
	{ 0x1.9d0807615c643p-2, 0x1.4be5f957778a1p-3, 0x1.724287f46debcp-1, 0x1.2bf516a59d19cp+0 },	// a = 49/128
	{ 0x1.a1b9f90318dcbp-2, 0x1.4ec973260026ap-3, 0x1.713786d9c7c09p-1, 0x1.2d04818244483p+0 },	// a = 99/256
	{ 0x1.a67031b542059p-2, 0x1.51aad872df82dp-3, 0x1.702e05c0b817p-1, 0x1.2e18a2423b269p+0 },	// a = 25/64
	{ 0x1.ab2ac4788f0dcp-2, 0x1.548a2c3add263p-3, 0x1.6f26016f26017p-1, 0x1.2f318d924a53cp+0 },	// a = 101/256
	{ 0x1.afe9c4a18b0e3p-2, 0x1.5767717455a6cp-3, 0x1.6e1f76b4337c7p-1, 0x1.304f58b05ffdp+0 },	// a = 51/128
	{ 0x1.b4ad45dae2d59p-2, 0x1.5a42ab0f4cfe2p-3, 0x1.6d1a62681c861p-1, 0x1.3172197032a26p+0 },	// a = 103/256
	{ 0x1.b9755c27c59ep-2, 0x1.5d1bdbf5809cap-3, 0x1.6c16c16c16c17p-1, 0x1.3299e6401329ap+0 },	// a = 13/32
	{ 0x1.be421be6596cbp-2, 0x1.5ff3070a793d4p-3, 0x1.6b1490aa31a3dp-1, 0x1.33c6d62df06fcp+0 },	// a = 105/256
	{ 0x1.c31399d243e72p-2, 0x1.62c82f2b9c795p-3, 0x1.6a13cd153729p-1, 0x1.34f900ec8ea4bp+0 },	// a = 53/128
	{ 0x1.c7e9eb074870bp-2, 0x1.659b57303e1f3p-3, 0x1.691473a88d0cp-1, 0x1.36307ed8f4df6p+0 },	// a = 107/256
	{ 0x1.ccc52503fc6fep-2, 0x1.686c81e9b14afp-3, 0x1.6816816816817p-1, 0x1.376d69001376dp+0 },	// a = 27/64
	{ 0x1.d1a55dac92a26p-2, 0x1.6b3bb2235943ep-3, 0x1.6719f3601671ap-1, 0x1.38afd924a5d42p+0 },	// a = 109/256
	{ 0x1.d68aab4dbe74bp-2, 0x1.6e08eaa2ba1e4p-3, 0x1.661ec6a5122f9p-1, 0x1.39f7e9c55292ep+0 },	// a = 55/128
	{ 0x1.db75249fb05b3p-2, 0x1.70d42e2789236p-3, 0x1.6524f853b4aa3p-1, 0x1.3b45b6230cf21p+0 },	// a = 111/256
	{ 0x1.e064e0c92c396p-2, 0x1.739d7f6bbd007p-3, 0x1.642c8590b2164p-1, 0x1.3c995a47babe7p+0 },	// a = 7/16
	{ 0x1.e559f762baeeep-2, 0x1.7664e1239dbcfp-3, 0x1.63356b88ac0dep-1, 0x1.3df2f30d221p+0 },	// a = 113/256
	{ 0x1.ea548079f8314p-2, 0x1.792a55fdd47a2p-3, 0x1.623fa7701624p-1, 0x1.3f529e2422615p+0 },	// a = 57/128
	{ 0x1.ef549494fde6bp-2, 0x1.7bede0a37afcp-3, 0x1.614b36831ae94p-1, 0x1.40b87a1c3cbdfp+0 },	// a = 115/256
	{ 0x1.f45a4cb5ee468p-2, 0x1.7eaf83b82afc3p-3, 0x1.6058160581606p-1, 0x1.4224a66b6ef9p+0 },	// a = 29/64
	{ 0x1.f965c25e9e132p-2, 0x1.816f41da0d496p-3, 0x1.5f66434292dfcp-1, 0x1.4397437666199p+0 },	// a = 117/256
	{ 0x1.fe770f9460541p-2, 0x1.842d1da1e8b17p-3, 0x1.5e75bb8d015e7p-1, 0x1.451072990c667p+0 },	// a = 59/128
	{ 0x1.01c72771fa832p-1, 0x1.86e919a330bap-3, 0x1.5d867c3ece2a5p-1, 0x1.4690562f77beep+0 },	// a = 119/256
	{ 0x1.0455cdb2ce279p-1, 0x1.89a3386c1425bp-3, 0x1.5c9882b931057p-1, 0x1.4817119f3d325p+0 },	// a = 15/32
	{ 0x1.06e78860a7e8cp-1, 0x1.8c5b7c858b48bp-3, 0x1.5babcc647fa91p-1, 0x1.49a4c9612f15ep+0 },	// a = 121/256
	{ 0x1.097c659991ec1p-1, 0x1.8f11e873662c7p-3, 0x1.5ac056b015acp-1, 0x1.4b39a30b8b264p+0 },	// a = 61/128
	{ 0x1.0c1473c7e911cp-1, 0x1.91c67eb45a83ep-3, 0x1.59d61f123ccaap-1, 0x1.4cd5c55c9e98bp+0 },	// a = 123/256
	{ 0x1.0eafc1a4b81eap-1, 0x1.947941c2116fbp-3, 0x1.58ed2308158edp-1, 0x1.4e795845e65bfp+0 },	// a = 31/64
	{ 0x1.114e5e3a29a89p-1, 0x1.972a341135158p-3, 0x1.580560158056p-1, 0x1.502484f7b2291p+0 },	// a = 125/256
	{ 0x1.13f058e611d13p-1, 0x1.99d958117e08bp-3, 0x1.571ed3c506b3ap-1, 0x1.51d775ed516dep+0 },	// a = 63/128
	{ 0x1.1695c15c90ea9p-1, 0x1.9c86b02dc0863p-3, 0x1.56397ba7c52e2p-1, 0x1.539256f9d18b1p+0 },	// a = 127/256
	{ 0x1.193ea7aad030bp-1, 0x1.9f323ecbf984cp-3, 0x1.5555555555555p-1, 0x1.5555555555555p+0 },	// a = 1/2
	{ 0x1.1beb1c39d9d1ap-1, 0x1.a1dc064d5b995p-3, 0x1.54725e6bb82fep-1, 0x1.57209fab0e4c8p+0 },	// a = 129/256
	{ 0x1.1e9b2fd18d91cp-1, 0x1.a484090e5bb0ap-3, 0x1.5390948f40febp-1, 0x1.58f46627e080bp+0 },	// a = 65/128
	{ 0x1.214ef39bb369dp-1, 0x1.a72a4966bd9eap-3, 0x1.52aff56a8054bp-1, 0x1.5ad0da89bab4fp+0 },	// a = 131/256
	{ 0x1.240679272d92cp-1, 0x1.a9cec9a9a084ap-3, 0x1.51d07eae2f815p-1, 0x1.5cb6302face89p+0 },	// a = 33/64
	{ 0x1.26c1d26b4b85p-1, 0x1.ac718c258b0e4p-3, 0x1.50f22e111c4c5p-1, 0x1.5ea49c2ac81d8p+0 },	// a = 133/256
	{ 0x1.298111cb3f8adp-1, 0x1.af1293247786bp-3, 0x1.5015015015015p-1, 0x1.609c554fd2e4p+0 },	// a = 67/128
	{ 0x1.2c444a19b89a5p-1, 0x1.b1b1e0ebdfc5bp-3, 0x1.4f38f62dd4c9bp-1, 0x1.629d9449defb6p+0 },	// a = 135/256
	{ 0x1.2f0b8e9ca2471p-1, 0x1.b44f77bcc8f63p-3, 0x1.4e5e0a72f0539p-1, 0x1.64a893adcd25fp+0 },	// a = 17/32
	{ 0x1.31d6f3110cb49p-1, 0x1.b6eb59d3cf35ep-3, 0x1.4d843bedc2c4cp-1, 0x1.66bd900ecd324p+0 },	// a = 137/256
	{ 0x1.34a68baf3e921p-1, 0x1.b9858969310fbp-3, 0x1.4cab88725af6ep-1, 0x1.68dcc813e92e9p+0 },	// a = 69/128
	{ 0x1.377a6d2ef3448p-1, 0x1.bc1e08b0dad0ap-3, 0x1.4bd3edda68fe1p-1, 0x1.6b067c8eabc0ap+0 },	// a = 139/256
	{ 0x1.3a52accbc786fp-1, 0x1.beb4d9da71b7cp-3, 0x1.4afd6a052bf5bp-1, 0x1.6d3af092f2b6dp+0 },	// a = 35/64
	{ 0x1.3d2f6049d6eb3p-1, 0x1.c149ff115f027p-3, 0x1.4a27fad76014ap-1, 0x1.6f7a69900016fp+0 },	// a = 141/256
	{ 0x1.40109dfa8ccb5p-1, 0x1.c3dd7a7cdad4dp-3, 0x1.49539e3b2d067p-1, 0x1.71c52f6add38ap+0 },	// a = 71/128
	{ 0x1.42f67cc1ab64cp-1, 0x1.c66f4e3ff6ff8p-3, 0x1.488052201488p-1, 0x1.741b8c9a24d98p+0 },	// a = 143/256
	{ 0x1.45e1141a8c01p-1, 0x1.c8ff7c79a9a22p-3, 0x1.47ae147ae147bp-1, 0x1.767dce434a9b1p+0 },	// a = 9/16
	{ 0x1.48d07c1d9b3eap-1, 0x1.cb8e0744d7acap-3, 0x1.46dce34596066p-1, 0x1.78ec445977f4fp+0 },	// a = 145/256
	{ 0x1.4bc4cd8614bf6p-1, 0x1.ce1af0b85f3ebp-3, 0x1.460cbc7f5cf9ap-1, 0x1.7b6741be18685p+0 },	// a = 73/128
	{ 0x1.4ebe21b801b3dp-1, 0x1.d0a63ae721e64p-3, 0x1.453d9e2c776cap-1, 0x1.7def1c6330a52p+0 },	// a = 147/256
	{ 0x1.51bc92c67dfa3p-1, 0x1.d32fe7e00ebd5p-3, 0x1.446f86562d9fbp-1, 0x1.80842d6f9e5e7p+0 },	// a = 37/64
	{ 0x1.54c03b7a47bfp-1, 0x1.d5b7f9ae2c684p-3, 0x1.43a2730abee4dp-1, 0x1.8326d16560c53p+0 },	// a = 149/256
	{ 0x1.57c937589dd3fp-1, 0x1.d83e7258a2f3ep-3, 0x1.42d6625d51f87p-1, 0x1.85d7684a0c0a3p+0 },	// a = 75/128
	{ 0x1.5ad7a2aa7137dp-1, 0x1.dac353e2c5954p-3, 0x1.420b5265e5951p-1, 0x1.889655d18ce68p+0 },	// a = 151/256
	{ 0x1.5deb9a83ee95cp-1, 0x1.dd46a04c1c4a1p-3, 0x1.4141414141414p-1, 0x1.8b64018b64019p+0 },	// a = 19/32
	{ 0x1.61053ccc64d7bp-1, 0x1.dfc859906d5b5p-3, 0x1.40782d10e6566p-1, 0x1.8e40d7128425cp+0 },	// a = 153/256
	{ 0x1.6424a8468e3dp-1, 0x1.e24881a7c6c26p-3, 0x1.3fb013fb013fbp-1, 0x1.912d464001913p+0 },	// a = 77/128
	{ 0x1.6749fc9941cbbp-1, 0x1.e4c71a8687704p-3, 0x1.3ee8f42a5af07p-1, 0x1.9429c360c45bdp+0 },	// a = 155/256
	{ 0x1.6a755a5893549p-1, 0x1.e744261d68788p-3, 0x1.3e22cbce4a902p-1, 0x1.9736c76e73ebcp+0 },	// a = 39/64
	{ 0x1.6da6e30f68b8ep-1, 0x1.e9bfa659861f5p-3, 0x1.3d5d991aa75c6p-1, 0x1.9a54d04bd5cccp+0 },	// a = 157/256
	{ 0x1.70deb9498b947p-1, 0x1.ec399d2468ccp-3, 0x1.3c995a47babe7p-1, 0x1.9d846104df033p+0 },	// a = 79/128
	{ 0x1.741d009e3ef5ap-1, 0x1.eeb20c640ddf4p-3, 0x1.3bd60d9232955p-1, 0x1.a0c60212bc26ap+0 },	// a = 159/256
	{ 0x1.7761ddbb61598p-1, 0x1.f128f5faf06edp-3, 0x1.3b13b13b13b14p-1, 0x1.a41a41a41a41ap+0 },	// a = 5/8
	{ 0x1.7aad767123bc7p-1, 0x1.f39e5bc811e5cp-3, 0x1.3a524387ac822p-1, 0x1.a781b3ea00af6p+0 },	// a = 161/256
	{ 0x1.7dfff1be5f383p-1, 0x1.f6123fa7028acp-3, 0x1.3991c2c187f63p-1, 0x1.aafcf3699303p+0 },	// a = 81/128
	{ 0x1.815977dd935afp-1, 0x1.f884a36fe9ec2p-3, 0x1.38d22d366088ep-1, 0x1.ae8ca15319829p+0 },	// a = 163/256
	{ 0x1.84ba3252982b5p-1, 0x1.faf588f78f31fp-3, 0x1.3813813813814p-1, 0x1.b23165deb6f69p+0 },	// a = 41/64
	{ 0x1.88224bf90fa2p-1, 0x1.fd64f20f61572p-3, 0x1.3755bd1c945eep-1, 0x1.b5ebf0af3b992p+0 },	// a = 165/256
	{ 0x1.8b91f113a34a3p-1, 0x1.ffd2e0857f498p-3, 0x1.3698df3de0748p-1, 0x1.b9bcf93b8ede9p+0 },	// a = 83/128
	{ 0x1.8f094f5c1bba9p-1, 0x1.011fab125ff8ap-2, 0x1.35dce5f9f2af8p-1, 0x1.bda53f3f34c2ep+0 },	// a = 167/256
	{ 0x1.9288961460abep-1, 0x1.02552a5a5d0ffp-2, 0x1.3521cfb2b78c1p-1, 0x1.c1a58b327f576p+0 },	// a = 21/32
	{ 0x1.960ff61871a12p-1, 0x1.0389eefce633bp-2, 0x1.34679ace01346p-1, 0x1.c5beaecb0a99ap+0 },	// a = 169/256
	{ 0x1.999fa1f16860cp-1, 0x1.04bdf9da926d2p-2, 0x1.33ae45b57bcb2p-1, 0x1.c9f185852f521p+0 },	// a = 85/128
	{ 0x1.9d37cde997e64p-1, 0x1.05f14bd26459cp-2, 0x1.32f5ced6a1dfap-1, 0x1.ce3ef53729f97p+0 },	// a = 171/256
	{ 0x1.a0d8b021dc00cp-1, 0x1.0723e5c1cdf4p-2, 0x1.323e34a2b10bfp-1, 0x1.d2a7eeaec4a48p+0 },	// a = 43/64
	{ 0x1.a48280a82f83ep-1, 0x1.0855c884b450ep-2, 0x1.3187758e9ebb6p-1, 0x1.d72d6e5a66e97p+0 },	// a = 173/256
	{ 0x1.a835798fa0cbap-1, 0x1.0986f4f573521p-2, 0x1.30d190130d19p-1, 0x1.dbd07cfe84d5ep+0 },	// a = 87/128
	{ 0x1.abf1d709be5fbp-1, 0x1.0ab76bece14d2p-2, 0x1.301c82ac4026p-1, 0x1.e09230787ea78p+0 },	// a = 175/256
	{ 0x1.afb7d78197bedp-1, 0x1.0be72e4252a83p-2, 0x1.2f684bda12f68p-1, 0x1.e573ac901e574p+0 },	// a = 11/16
	{ 0x1.b387bbb870d4dp-1, 0x1.0d163ccb9d6b8p-2, 0x1.2eb4ea1fed14bp-1, 0x1.ea7623d8fe82cp+0 },	// a = 177/256
	{ 0x1.b761c6e44955ep-1, 0x1.0e44985d1cc8cp-2, 0x1.2e025c04b8097p-1, 0x1.ef9ad8a54844p+0 },	// a = 89/128
	{ 0x1.bb463ed05c38dp-1, 0x1.0f7241c9b497dp-2, 0x1.2d50a012d50ap-1, 0x1.f4e31e0b5b7e6p+0 },	// a = 179/256
	{ 0x1.bf356bffbedcp-1, 0x1.109f39e2d4c97p-2, 0x1.2c9fb4d812cap-1, 0x1.fa5059001fa5p+0 },	// a = 45/64
	{ 0x1.c32f99d24b0c8p-1, 0x1.11cb81787ccf8p-2, 0x1.2bef98e5a3711p-1, 0x1.ffe40187ea913p+0 },	// a = 181/256
	{ 0x1.c73516ac03329p-1, 0x1.12f719593efbcp-2, 0x1.2b404ad012b4p-1, 0x1.02cfd200102dp+1 },	// a = 91/128
	{ 0x1.cb46341f246c6p-1, 0x1.1422025243d45p-2, 0x1.2a91c92f3c105p-1, 0x1.05c27141f5891p+1 },	// a = 183/256
	{ 0x1.cf6347191f5b5p-1, 0x1.154c3d2f4d5eap-2, 0x1.29e4129e4129ep-1, 0x1.08cabb37565e2p+1 },	// a = 23/32
	{ 0x1.d38ca812b5fap-1, 0x1.1675cababa60ep-2, 0x1.293725bb804a5p-1, 0x1.0be998ff8ce1ap+1 },	// a = 185/256
	{ 0x1.d7c2b3438300bp-1, 0x1.179eabbd899a1p-2, 0x1.288b01288b013p-1, 0x1.0f20010f20011p+1 },	// a = 93/128
	{ 0x1.dc05c8d936455p-1, 0x1.18c6e0ff5cf06p-2, 0x1.27dfa38a1ce4dp-1, 0x1.126ef8270fab7p+1 },	// a = 187/256
	{ 0x1.e0564d32d9391p-1, 0x1.19ee6b467c96fp-2, 0x1.27350b8812735p-1, 0x1.15d79261f33f6p+1 },	// a = 47/64
	{ 0x1.e4b4a92077457p-1, 0x1.1b154b57da29fp-2, 0x1.268b37cd60127p-1, 0x1.195af45931bd8p+1 },	// a = 189/256
	{ 0x1.e9214a278f736p-1, 0x1.1c3b81f713c25p-2, 0x1.25e22708092f1p-1, 0x1.1cfa5464e21e2p+1 },	// a = 95/128
	{ 0x1.ed9ca2ccbf9e9p-1, 0x1.1d610fe677003p-2, 0x1.2539d7e9177b2p-1, 0x1.20b6fbf932b06p+1 },	// a = 191/256
	{ 0x1.f2272ae325a57p-1, 0x1.1e85f5e7040dp-2, 0x1.2492492492492p-1, 0x1.2492492492492p+1 },	// a = 3/4
	{ 0x1.f6c15fe200bf7p-1, 0x1.1faa34b87094cp-2, 0x1.23eb79717605bp-1, 0x1.288db0323f01cp+1 },	// a = 193/256
	{ 0x1.fb6bc5412c9c8p-1, 0x1.20cdcd192ab6ep-2, 0x1.23456789abcdfp-1, 0x1.2caabd755682p+1 },	// a = 97/128
	{ 0x1.0013726e90b8p+0, 0x1.21f0bfc65beecp-2, 0x1.22a0122a0122ap-1, 0x1.30eb17410dc8p+1 },	// a = 195/256
	{ 0x1.0279a7b19be98p+0, 0x1.23130d7bebf43p-2, 0x1.21fb78121fb78p-1, 0x1.355080135508p+1 },	// a = 49/64
	{ 0x1.04e8ce6382f3ap+0, 0x1.2434b6f483934p-2, 0x1.21579804855e6p-1, 0x1.39dcd8f7e31cap+1 },	// a = 197/256
	{ 0x1.07613660e2537p+0, 0x1.2555bce98f7cbp-2, 0x1.20b470c67c0d9p-1, 0x1.3e92242a773b1p+1 },	// a = 99/128
	{ 0x1.09e333ae64b65p+0, 0x1.26762013430ep-2, 0x1.2012012012012p-1, 0x1.4372880014373p+1 },	// a = 199/256
	{ 0x1.0c6f1ec420786p+0, 0x1.2795e1289b11bp-2, 0x1.1f7047dc11f7p-1, 0x1.488052201488p+1 },	// a = 25/32
	{ 0x1.0f0554dfbf7dap+0, 0x1.28b500df60783p-2, 0x1.1ecf43c7fb84cp-1, 0x1.4dbdfb17409a8p+1 },	// a = 201/256
	{ 0x1.11a6385e30cf6p+0, 0x1.29d37fec2b08bp-2, 0x1.1e2ef3b3fb874p-1, 0x1.532e2a5092677p+1 },	// a = 101/128
	{ 0x1.1452311dbc75fp+0, 0x1.2af15f02640adp-2, 0x1.1d8f5672e4abdp-1, 0x1.58d3ba8114219p+1 },	// a = 203/256
	{ 0x1.1709ace96ee39p+0, 0x1.2c0e9ed448e8cp-2, 0x1.1cf06ada2811dp-1, 0x1.5eb1be9658b37p+1 },	// a = 51/64
	{ 0x1.19cd1feef2a07p+0, 0x1.2d2b4012edc9ep-2, 0x1.1c522fc1ce059p-1, 0x1.64cb87397b01fp+1 },	// a = 205/256
	{ 0x1.1c9d0540158c9p+0, 0x1.2e47436e40268p-2, 0x1.1bb4a4046ed29p-1, 0x1.6b24a8fb6f22p+1 },	// a = 103/128
	{ 0x1.1f79df6163ed8p+0, 0x1.2f62a99509546p-2, 0x1.1b17c67f2bae3p-1, 0x1.71c10342d5c96p+1 },	// a = 207/256
	{ 0x1.226438e777c99p+0, 0x1.307d7334f10bep-2, 0x1.1a7b9611a7b96p-1, 0x1.78a4c8178a4c8p+1 },	// a = 13/16
	{ 0x1.255ca524d82c3p+0, 0x1.3197a0fa7fe6ap-2, 0x1.19e0119e0119ep-1, 0x1.7fd484ecf128bp+1 },	// a = 209/256
	{ 0x1.2863c0ea8b8cfp+0, 0x1.32b1339121d71p-2, 0x1.19453808ca29cp-1, 0x1.87552c91cb5b8p+1 },	// a = 105/128
	{ 0x1.2b7a335dd4c9fp+0, 0x1.33ca2ba328995p-2, 0x1.18ab083902bdbp-1, 0x1.8f2c227337192p+1 },	// a = 211/256
	{ 0x1.2ea0aee5f581p+0, 0x1.34e289d9ce1d3p-2, 0x1.1811811811812p-1, 0x1.975f4768d3a48p+1 },	// a = 53/64
	{ 0x1.31d7f23546375p+0, 0x1.35fa4edd36eap-2, 0x1.1778a191bd684p-1, 0x1.9ff5084a080c5p+1 },	// a = 213/256
	{ 0x1.3520c9718089fp+0, 0x1.37117b54747b6p-2, 0x1.16e0689427379p-1, 0x1.a8f46e989d487p+1 },	// a = 107/128
	{ 0x1.387c0f7fbe5f1p+0, 0x1.38280fe58797fp-2, 0x1.1648d50fc3201p-1, 0x1.b265339bb9f3ap+1 },	// a = 215/256
	{ 0x1.3beaaf7978c58p+0, 0x1.393e0d3562a1ap-2, 0x1.15b1e5f75270dp-1, 0x1.bc4fd65883e7bp+1 },	// a = 27/32
	{ 0x1.3f6da650c1586p+0, 0x1.3a5373e7ebdfap-2, 0x1.151b9a3fdd5c9p-1, 0x1.c6bdb4ec15ed3p+1 },	// a = 217/256
	{ 0x1.430604ab1313fp+0, 0x1.3b68449fffc23p-2, 0x1.1485f0e0acd3bp-1, 0x1.d1b929e6308dp+1 },	// a = 109/128
	{ 0x1.46b4f0fb778a3p+0, 0x1.3c7c7fff73206p-2, 0x1.13f0e8d344724p-1, 0x1.dd4dae66843b4p+1 },	// a = 219/256
	{ 0x1.4a7ba9e66a99bp+0, 0x1.3d9026a7156fbp-2, 0x1.135c81135c811p-1, 0x1.e98801e98801fp+1 },	// a = 55/64
	{ 0x1.4e5b88fbf4bfp+0, 0x1.3ea33936b2f5cp-2, 0x1.12c8b89edc0acp-1, 0x1.f67658e7f8c33p+1 },	// a = 221/256
	{ 0x1.525605d6fc1b9p+0, 0x1.3fb5b84d16f42p-2, 0x1.12358e75d3033p-1, 0x1.021449d84e212p+2 },	// a = 111/128
	{ 0x1.566cb9b3ef248p+0, 0x1.40c7a4880dce9p-2, 0x1.11a3019a74826p-1, 0x1.09583f9d88406p+2 },	// a = 223/256
	{ 0x1.5aa16394d481fp+0, 0x1.41d8fe84672aep-2, 0x1.1111111111111p-1, 0x1.1111111111111p+2 },	// a = 7/8
	{ 0x1.5ef5ed0db28b4p+0, 0x1.42e9c6ddf80bfp-2, 0x1.107fbbe01108p-1, 0x1.194a0c422218cp+2 },	// a = 225/256
	{ 0x1.636c6fda710ddp+0, 0x1.43f9fe2f9ce67p-2, 0x1.0fef010fef011p-1, 0x1.2210012210012p+2 },	// a = 113/128
	{ 0x1.68073c6736482p+0, 0x1.4509a5133bb0ap-2, 0x1.0f5edfab325a2p-1, 0x1.2b71840c5adfp+2 },	// a = 227/256
	{ 0x1.6cc8e17e5416cp+0, 0x1.4618bc21c5ec2p-2, 0x1.0ecf56be69c9p-1, 0x1.357f3e9078e5bp+2 },	// a = 57/64
	{ 0x1.71b4355bdd9fap+0, 0x1.472743f33aaadp-2, 0x1.0e40655826011p-1, 0x1.404c522f95569p+2 },	// a = 229/256
	{ 0x1.76cc6077efb4ap+0, 0x1.48353d1ea88dfp-2, 0x1.0db20a88f4696p-1, 0x1.4beed1e3a2f7dp+2 },	// a = 115/128
	{ 0x1.7c14ea6efa5adp+0, 0x1.4942a83a2fc07p-2, 0x1.0d24456359e3ap-1, 0x1.588058d116e5ep+2 },	// a = 231/256
	{ 0x1.8191c98ce5694p+0, 0x1.4a4f85db03ebbp-2, 0x1.0c9714fbcda3bp-1, 0x1.661ec6a5122f9p+2 },	// a = 29/32
	{ 0x1.874775a78788bp+0, 0x1.4b5bd6956e274p-2, 0x1.0c0a7868b4171p-1, 0x1.74ed2d173d57bp+2 },	// a = 233/256
	{ 0x1.8d3aff2a9e9a5p+0, 0x1.4c679afccee3ap-2, 0x1.0b7e6ec259dc8p-1, 0x1.8514fe31f7122p+2 },	// a = 117/128
	{ 0x1.93722b813446dp+0, 0x1.4d72d3a39fdp-2, 0x1.0af2f722eecb5p-1, 0x1.96c790f8474e4p+2 },	// a = 235/256
	{ 0x1.99f3986ee6739p+0, 0x1.4e7d811b75bb1p-2, 0x1.0a6810a6810a7p-1, 0x1.aa401aa401aa4p+2 },	// a = 59/64
	{ 0x1.a0c6e87c3ca6cp+0, 0x1.4f87a3f5026e9p-2, 0x1.09ddba6af836p-1, 0x1.bfc64770ca75ep+2 },	// a = 237/256
	{ 0x1.a7f4fb6893c73p+0, 0x1.50913cc01686bp-2, 0x1.0953f39010954p-1, 0x1.d7b1b1001d7b2p+2 },	// a = 119/128
	{ 0x1.af8836c288176p+0, 0x1.519a4c0ba3446p-2, 0x1.08cabb37565e2p-1, 0x1.f26e8d955747cp+2 },	// a = 239/256
	{ 0x1.b78ce48912b5ap+0, 0x1.52a2d265bc5abp-2, 0x1.0842108421084p-1, 0x1.0842108421084p+3 },	// a = 15/16
	{ 0x1.c011b0615929fp+0, 0x1.53aad05b99b7dp-2, 0x1.07b9f29b8eae2p-1, 0x1.194ee0a5ed868p+3 },	// a = 241/256
	{ 0x1.c928501031d5dp+0, 0x1.54b2467999498p-2, 0x1.073260a47f7c6p-1, 0x1.2ccbdc29b645p+3 },	// a = 121/128
	{ 0x1.d2e66a7efea81p+0, 0x1.55b9354b40bcdp-2, 0x1.06ab59c7912fbp-1, 0x1.43490c09503acp+3 },	// a = 243/256
	{ 0x1.dd66db68dd248p+0, 0x1.56bf9d5b3f399p-2, 0x1.0624dd2f1a9fcp-1, 0x1.5d867c3ece2a5p+3 },	// a = 61/64
	{ 0x1.e8cb84342f8f7p+0, 0x1.57c57f336f191p-2, 0x1.059eea0727586p-1, 0x1.7c8a0e960aaf2p+3 },	// a = 245/256
	{ 0x1.f53ffafb27d2bp+0, 0x1.58cadb5cd7989p-2, 0x1.05197f7d73404p-1, 0x1.a1c265958533ap+3 },	// a = 123/128
	{ 0x1.017ed2663157ap+1, 0x1.59cfb25fae87ep-2, 0x1.04949cc1664c5p-1, 0x1.cf4116ad27a43p+3 },	// a = 247/256
	{ 0x1.09291e8e3181bp+1, 0x1.5ad404c359f2dp-2, 0x1.041041041041p-1, 0x1.041041041041p+4 },	// a = 31/32
	{ 0x1.11d55f98a86e8p+1, 0x1.5bd7d30e71c73p-2, 0x1.038c6b78247fcp-1, 0x1.28a07ad272db2p+4 },	// a = 249/256
	{ 0x1.1bd363bfd34dbp+1, 0x1.5cdb1dc6c1765p-2, 0x1.03091b51f5e1ap-1, 0x1.596179c29d2cep+4 },	// a = 125/128
	{ 0x1.279ee4be169c7p+1, 0x1.5ddde57149923p-2, 0x1.02864fc7729e9p-1, 0x1.9da3b2d8b7641p+4 },	// a = 251/256
	{ 0x1.3607294602e42p+1, 0x1.5ee02a9241675p-2, 0x1.0204081020408p-1, 0x1.0204081020408p+5 },	// a = 63/64
	{ 0x1.4890c3ba92a5p+1, 0x1.5fe1edad18919p-2, 0x1.0182436517a37p-1, 0x1.575859dc1f84ap+5 },	// a = 253/256
	{ 0x1.62a40fda3e3ccp+1, 0x1.60e32f44788d9p-2, 0x1.010101010101p-1, 0x1.010101010101p+6 },	// a = 127/128
	{ 0x1.8f20adeaec67cp+1, 0x1.61e3efda46467p-2, 0x1.008040201008p-1, 0x1.008040201008p+7 },	// a = 255/256
};


float atanhf(float x) {
	static const double one_plus_eps = 0x1.0000000000001p0;		// 1 + ulp
	static const double one_third =  0.33333333333333333;		// 1/3
	static const double one_fifth =  0.2;						// 1/5
	static const double neglog2_2 = -0.34657359027997264;		// -0.5 * log(2)
	
	if (x != x) return x + x;	// deal with NaN
	double fabsx = __builtin_fabs((double)x);
	double result;
	
	// Small |x| are handled via very simple approximations.
	if (fabsx <= 0x1.0p-5) {
		
		// For very small |x|, just multiply by 1 + eps to get flags and rounding.
		if (fabsx <= 0x1.0p-13)
			result = fabsx * one_plus_eps;
		
		// small Taylor series works up to 1/32, avoids doing lookups.
		else
			result = fabsx + (fabsx*fabsx)*(one_third*fabsx + (one_fifth*fabsx)*(fabsx*fabsx));
	}
	
	else if (fabsx < 0x1.cp-1) {
		
		/*
		 (Set with tab = 4 spaces)
		 
		 For 1/32 < x < 7/8:
		 
			Compute a = floor(x*256), set hi = a/256.  a is an integer between 0 and 223.  hi is an approximation to
			x, with
		 
			(1)							0 < (x - a) < 1/256
		 
			We base our approximation on the identity
		 
														1 + x
			(2)						atanh(x) = 1/2 log ------- = 1/2 ( log(1+x) - log(1-x) )
														1 - x
		 
			Expand 1±x as follows:
		 
					(1 + x) = (1 + hi)(1 + even + odd)			(1 - x) = (1 - hi)(1 + even - odd)
		 
			and solve for b,c to get:
			
											  x - a
										c = ---------			b = -ac
											 1 - a^2
		 
			Substituting into (2), we get:
		 
					atanh(x) = 1/2 ( log(1 + hi) + log(1 + even + odd) - log(1 - hi) - log(1 + even - odd) )
		 
			(3)				 = 1/2 atanh(hi) + 1/2 ( log(1 + even + odd) - log(1 + even - odd) )
		 
			We can store atanh(hi), and also the 1/(1-hi^2) factor used to compute odd, in a lookup table indexed on a.
			A fairly straighforward analysis using (1) and the fact that a < 7/8 shows that
		 
										|b|, |c| < 1/60
		 
			So a 4th-order taylor series will approximate the second term of (3) to very nearly the desired accuracy.
			When we compute the series, some really truly wonderful cancellation takes place, leaving us with a beautiful
			approximation:
		 
			1/2 (log(1+even+odd) - log(1+even-odd)) = odd - odd*even + odd*even^2 + 1/3 odd^3 - odd*even^3 - odd^3*even
													= odd((1 - even)(1 + even^2) + (1/3 - even)odd^2)
		 
			Unfortunately, this is not *quite* accurate enough - it results in errors of approximately .75 ulps.  So we
			tweak the approximation ever so slightly to achieve the necessary accuracy:
		 
				1/2 (log(1+even+odd) - log(1+even-odd)) = odd((1 - even)(1 + even^2) + (1/3 - 1.04 even)odd^2)
		 
			Putting this all together, we get
		 
			(4)		atanh(x) = atanh(a) + odd((1 - even)(1 + even^2) + (1/3 - 1.04 even)odd^2) + (< .51 ulp)
		 
			- Stephen Canon, July 2007
		 */
		
		double hi = fabsx * 256.0;
		int a = (int)hi;
		hi = (double)a * 0x1.0p-8;
		
		double odd = (fabsx - hi) * atanhf_table[a].one_minus_hisquared_recip;
		double even = hi * odd;	// this is actually -even in the analysis above, but it saves a couple cycles.
		
		// (4) from above using -even instead of even
		result = (odd*((1.0 + even)*(1.0 + even*even) + (one_third + 1.04*even)*odd*odd)) + atanhf_table[a].atanh_hi;
	}
	
	else if (fabsx < 0x1.0p0) {
		/*
		 (Set with tab = 4 spaces)
		 
		 For 7/8 <= |x| < 1:
		 
		 On this range, we use an approximation of the form:
		 
		 (1)		atanh(x) = -1/2 log((1-x)/2) - minimax polynomial in (1-x)
		 
		 The second term of (1) is just a straightforward 4-term minimax polynomial.  To compute the first term, consider
		 
					(1-x) = mantissa * 2^exponent
		 
		 with 1.0 <= mantissa < 2.  Set a = floor(256(mantissa - 1)), and let hi = a/256 and lo = (x-hi)/(1+hi).  Then:
		 
		 (2)        -1/2 log((1-x)/2) = -ln2/2 * (exponent-1) - 1/2 log(1+hi) - 1/2 log(1+lo)
		 
		 The first term can be computed directly; the second term is looked up in the table indexed on a, and the final
		 term is approximated via another minimax polynomial.
		 
		 - Stephen Canon, July 2007
		
		*/
		double one_minus_x = 1.0 - fabsx;
		
		union { double d; uint64_t x; } frac = { one_minus_x };
		int exponent = (int32_t)(frac.x >> 52) - 1023;
		frac.x = (frac.x & 0x000fffffffffffffULL) | 0x3ff0000000000000ULL; // frac has the mantissa of x, with exponent 0
		frac.d -= 1.0;
		
		double hi = frac.d * 256.0;
		int a = (int)hi;
		hi = (double)a * 0x1.0p-8;
		
		double lo = (frac.d - hi) * atanhf_table[a].one_plus_hi_recip;
		result = ((0.12512217836005105*lo - 0.16676438694257623)*lo + 0.24999996021630813)*(lo*lo) - 0.49999998806861079*lo;
		result += (double)(exponent-1)*neglog2_2 - atanhf_table[a].half_log1p_hi; // = -0.5*log1p(0.5*(1-x))
		
		result -= (((0.0088812*one_minus_x + 0.0207128)*one_minus_x + 0.062505472)*one_minus_x + 0.249999915)*one_minus_x;
	}
	
	else if (fabsx > 1.0) {
		union { double d; uint64_t x; } tmp = { 0x7ff0000000000000ULL };
		result = tmp.d - tmp.d; // inf - inf => invalid
	}
	
	else {
		result = 1.0 / (fabsx - 1.0); // fabsx = 1.0, division by zero, return inf.
	}
	
	if (x < 0.0f) result = -result;
	return (float)result;
}
