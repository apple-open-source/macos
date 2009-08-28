//
//	powf.c
//
//		by Ian Ollmann
//
//	Copyright (c) 2007, Apple Inc. All Rights Reserved.
//



#include <math.h>
#include <stdint.h>

// used for testing if a float is an integer or not
static const uint8_t  gMaskShift[256] = {	0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //16
											0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //32
											0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //48
											0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //64
											0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //80
											0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //96
											0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //112
											0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7,    //128
											8, 9,10,11, 12,13,14,15,16,17,18,19,20,21,22,23,    //144
										   24,25,26,27, 28,29,30,31,31,31,31,31,31,31,31,31,    //160
										   31,31,31,31, 31,31,31,31,31,31,31,31,31,31,31,31,    //176
										   31,31,31,31, 31,31,31,31,31,31,31,31,31,31,31,31,    //192
										   31,31,31,31, 31,31,31,31,31,31,31,31,31,31,31,31,    //208
										   31,31,31,31, 31,31,31,31,31,31,31,31,31,31,31,31,    //224
										   31,31,31,31, 31,31,31,31,31,31,31,31,31,31,31,31,    //240
										   31,31,31,31, 31,31,31,31,31,31,31,31,31,31,31,31,    //256
										};

// Table of  1/(1+a/127), log2( 1 + a/127 )
//    produced by:
//
//		#include <stdio.h>
//		#include <stdint.h>
//		#include <math.h>
//		
//		int main( void )
//		{
//			int i;
//		
//			for( i = 0; i < 128; i++ )
//			{	
//				long double a = 1.0L /( 1.0L + (long double) i / (long double) 127 );
//		
//				printf( "%a,\t%a,\t// %Lg, -log2l(%Lg)\n", (double) a, (double) -log2l( a ), a, a );
//			}
//		
//			return 0;
//		}
//
// We use 127 rather than 128 here so that the two end points that need extra accuracy x E {1.0f+ulp, 1.0f-ulp}
// end up being reduced by exact powers of two and accumulate no rounding error in reduction. We use the Taylor
// polynomial for log(1+r) to produce very high precision calculations for these values. Values outside the
// range [1.0-2**-7, 1.0+2**-7] require far less accuracy. For these, the 5th order Taylor polynomial should be good to 
// at least (5+1)*7 = 42 bits of precision, which should be good enough. 0x1.02 ** y overflows at around 10,000, meaning 
// we need  around 24+14=38 bits of precsion. 

static const double powf_log_table[256] = {
											0x1p+0,	-0x0p+0,	// 1, -log2l(1)
											0x1.fcp-1,	0x1.72c7ba20f7327p-7,	// 0.992188, -log2l(0.992188)
											0x1.f80fe03f80fep-1,	0x1.715662c7f3dbcp-6,	// 0.984496, -log2l(0.984496)
											0x1.f42f42f42f42fp-1,	0x1.13eea2b6545dfp-5,	// 0.976923, -log2l(0.976923)
											0x1.f05dcd30dadecp-1,	0x1.6e7f0bd9710ddp-5,	// 0.969466, -log2l(0.969466)
											0x1.ec9b26c9b26cap-1,	0x1.c85f25e12da51p-5,	// 0.962121, -log2l(0.962121)
											0x1.e8e6fa39be8e7p-1,	0x1.10c8cd0c74414p-4,	// 0.954887, -log2l(0.954887)
											0x1.e540f4898d5f8p-1,	0x1.3d0c813e48ep-4,	// 0.947761, -log2l(0.947761)
											0x1.e1a8c536fe1a9p-1,	0x1.68fbf5169e028p-4,	// 0.940741, -log2l(0.940741)
											0x1.de1e1e1e1e1e2p-1,	0x1.949866f0b017bp-4,	// 0.933824, -log2l(0.933824)
											0x1.daa0b3630957dp-1,	0x1.bfe30e28821cp-4,	// 0.927007, -log2l(0.927007)
											0x1.d7303b5cc0ed7p-1,	0x1.eadd1b4ef9a1fp-4,	// 0.92029, -log2l(0.92029)
											0x1.d3cc6e80ebbdbp-1,	0x1.0ac3dc2e0ca0cp-3,	// 0.913669, -log2l(0.913669)
											0x1.d075075075075p-1,	0x1.1ff2046fb7116p-3,	// 0.907143, -log2l(0.907143)
											0x1.cd29c244fe2f3p-1,	0x1.34f99517622aep-3,	// 0.900709, -log2l(0.900709)
											0x1.c9ea5dbf193d5p-1,	0x1.49db19c99a54dp-3,	// 0.894366, -log2l(0.894366)
											0x1.c6b699f5423cep-1,	0x1.5e971b3a4ee8p-3,	// 0.888112, -log2l(0.888112)
											0x1.c38e38e38e38ep-1,	0x1.732e1f41ccdbap-3,	// 0.881944, -log2l(0.881944)
											0x1.c070fe3c070fep-1,	0x1.87a0a8f0ff9b2p-3,	// 0.875862, -log2l(0.875862)
											0x1.bd5eaf57abd5fp-1,	0x1.9bef38a4ffae5p-3,	// 0.869863, -log2l(0.869863)
											0x1.ba5713280dee9p-1,	0x1.b01a4c19f6811p-3,	// 0.863946, -log2l(0.863946)
											0x1.b759f2298375ap-1,	0x1.c4225e7d5e3c6p-3,	// 0.858108, -log2l(0.858108)
											0x1.b4671655e7f24p-1,	0x1.d807e87fa4521p-3,	// 0.852349, -log2l(0.852349)
											0x1.b17e4b17e4b18p-1,	0x1.ebcb6065350a2p-3,	// 0.846667, -log2l(0.846667)
											0x1.ae9f5d3eba7d7p-1,	0x1.ff6d3a16f617fp-3,	// 0.84106, -log2l(0.84106)
											0x1.abca1af286bcap-1,	0x1.0976f3991af9ep-2,	// 0.835526, -log2l(0.835526)
											0x1.a8fe53a8fe53bp-1,	0x1.1326eb8c0aba3p-2,	// 0.830065, -log2l(0.830065)
											0x1.a63bd81a98ef6p-1,	0x1.1cc6bb7e3870fp-2,	// 0.824675, -log2l(0.824675)
											0x1.a3827a3827a38p-1,	0x1.265698fa26c0ap-2,	// 0.819355, -log2l(0.819355)
											0x1.a0d20d20d20d2p-1,	0x1.2fd6b881e82d3p-2,	// 0.814103, -log2l(0.814103)
											0x1.9e2a65187566cp-1,	0x1.39474d95e1649p-2,	// 0.808917, -log2l(0.808917)
											0x1.9b8b577e61371p-1,	0x1.42a88abb54986p-2,	// 0.803797, -log2l(0.803797)
											0x1.98f4bac46d7cp-1,	0x1.4bfaa182b7fe3p-2,	// 0.798742, -log2l(0.798742)
											0x1.9666666666666p-1,	0x1.553dc28dd9724p-2,	// 0.79375, -log2l(0.79375)
											0x1.93e032e1c9f02p-1,	0x1.5e721d95d124dp-2,	// 0.78882, -log2l(0.78882)
											0x1.9161f9add3c0dp-1,	0x1.6797e170c5221p-2,	// 0.783951, -log2l(0.783951)
											0x1.8eeb9533d4065p-1,	0x1.70af3c177f74p-2,	// 0.779141, -log2l(0.779141)
											0x1.8c7ce0c7ce0c8p-1,	0x1.79b85aaad8878p-2,	// 0.77439, -log2l(0.77439)
											0x1.8a15b8a15b8a1p-1,	0x1.82b36978f76d5p-2,	// 0.769697, -log2l(0.769697)
											0x1.87b5f9d4d1bc2p-1,	0x1.8ba09402697edp-2,	// 0.76506, -log2l(0.76506)
											0x1.855d824ca58e9p-1,	0x1.948004ff12dbfp-2,	// 0.760479, -log2l(0.760479)
											0x1.830c30c30c30cp-1,	0x1.9d51e662f92a2p-2,	// 0.755952, -log2l(0.755952)
											0x1.80c1e4bbd595fp-1,	0x1.a6166162e9ec8p-2,	// 0.751479, -log2l(0.751479)
											0x1.7e7e7e7e7e7e8p-1,	0x1.aecd9e78fdbeap-2,	// 0.747059, -log2l(0.747059)
											0x1.7c41df1077c42p-1,	0x1.b777c568f9ae2p-2,	// 0.74269, -log2l(0.74269)
											0x1.7a0be82fa0be8p-1,	0x1.c014fd448fe3ap-2,	// 0.738372, -log2l(0.738372)
											0x1.77dc7c4cf2aeap-1,	0x1.c8a56c6f80bcap-2,	// 0.734104, -log2l(0.734104)
											0x1.75b37e875b37fp-1,	0x1.d12938a39d6fp-2,	// 0.729885, -log2l(0.729885)
											0x1.7390d2a6c405ep-1,	0x1.d9a086f4ad416p-2,	// 0.725714, -log2l(0.725714)
											0x1.71745d1745d17p-1,	0x1.e20b7bd4365a8p-2,	// 0.721591, -log2l(0.721591)
											0x1.6f5e02e4850ffp-1,	0x1.ea6a3b152b1e6p-2,	// 0.717514, -log2l(0.717514)
											0x1.6d4da9b536a6dp-1,	0x1.f2bce7ef7d06bp-2,	// 0.713483, -log2l(0.713483)
											0x1.6b4337c6cb157p-1,	0x1.fb03a50395dbap-2,	// 0.709497, -log2l(0.709497)
											0x1.693e93e93e93fp-1,	0x1.019f4a2edc134p-1,	// 0.705556, -log2l(0.705556)
											0x1.673fa57b0cbabp-1,	0x1.05b6ebbca3d9ap-1,	// 0.701657, -log2l(0.701657)
											0x1.6546546546546p-1,	0x1.09c8c7a1fd74cp-1,	// 0.697802, -log2l(0.697802)
											0x1.63528917c80b3p-1,	0x1.0dd4ee107ae0ap-1,	// 0.693989, -log2l(0.693989)
											0x1.61642c8590b21p-1,	0x1.11db6ef5e7873p-1,	// 0.690217, -log2l(0.690217)
											0x1.5f7b282135f7bp-1,	0x1.15dc59fdc06b7p-1,	// 0.686486, -log2l(0.686486)
											0x1.5d9765d9765d9p-1,	0x1.19d7be92a231p-1,	// 0.682796, -log2l(0.682796)
											0x1.5bb8d015e75bcp-1,	0x1.1dcdabdfad537p-1,	// 0.679144, -log2l(0.679144)
											0x1.59df51b3bea36p-1,	0x1.21be30d1e0ddbp-1,	// 0.675532, -log2l(0.675532)
											0x1.580ad602b580bp-1,	0x1.25a95c196bef3p-1,	// 0.671958, -log2l(0.671958)
											0x1.563b48c20563bp-1,	0x1.298f3c2af6595p-1,	// 0.668421, -log2l(0.668421)
											0x1.5470961d7ca63p-1,	0x1.2d6fdf40e09c5p-1,	// 0.664921, -log2l(0.664921)
											0x1.52aaaaaaaaaabp-1,	0x1.314b535c7b89ep-1,	// 0.661458, -log2l(0.661458)
											0x1.50e97366227cbp-1,	0x1.3521a64737cf3p-1,	// 0.658031, -log2l(0.658031)
											0x1.4f2cddb0d3225p-1,	0x1.38f2e593cda73p-1,	// 0.654639, -log2l(0.654639)
											0x1.4d74d74d74d75p-1,	0x1.3cbf1e9f5cf2fp-1,	// 0.651282, -log2l(0.651282)
											0x1.4bc14e5e0a72fp-1,	0x1.40865e9285f33p-1,	// 0.647959, -log2l(0.647959)
											0x1.4a1231617641p-1,	0x1.4448b2627ade3p-1,	// 0.64467, -log2l(0.64467)
											0x1.48676f31219dcp-1,	0x1.480626d20a876p-1,	// 0.641414, -log2l(0.641414)
											0x1.46c0f6feb6ac6p-1,	0x1.4bbec872a4505p-1,	// 0.638191, -log2l(0.638191)
											0x1.451eb851eb852p-1,	0x1.4f72a3a555958p-1,	// 0.635, -log2l(0.635)
											0x1.4380a3065e3fbp-1,	0x1.5321c49bc0c91p-1,	// 0.631841, -log2l(0.631841)
											0x1.41e6a74981447p-1,	0x1.56cc37590e6c5p-1,	// 0.628713, -log2l(0.628713)
											0x1.4050b59897548p-1,	0x1.5a7207b2d815ap-1,	// 0.625616, -log2l(0.625616)
											0x1.3ebebebebebecp-1,	0x1.5e1341520dbp-1,	// 0.622549, -log2l(0.622549)
											0x1.3d30b3d30b3d3p-1,	0x1.61afefb3d5201p-1,	// 0.619512, -log2l(0.619512)
											0x1.3ba68636adfbp-1,	0x1.65481e2a6477bp-1,	// 0.616505, -log2l(0.616505)
											0x1.3a2027932b48fp-1,	0x1.68dbd7ddd6e15p-1,	// 0.613527, -log2l(0.613527)
											0x1.389d89d89d89ep-1,	0x1.6c6b27ccfc698p-1,	// 0.610577, -log2l(0.610577)
											0x1.371e9f3c04e64p-1,	0x1.6ff618ce24cd7p-1,	// 0.607656, -log2l(0.607656)
											0x1.35a35a35a35a3p-1,	0x1.737cb58fe5716p-1,	// 0.604762, -log2l(0.604762)
											0x1.342bad7f64b39p-1,	0x1.76ff0899daa49p-1,	// 0.601896, -log2l(0.601896)
											0x1.32b78c13521dp-1,	0x1.7a7d1c4d6452p-1,	// 0.599057, -log2l(0.599057)
											0x1.3146e92a10d38p-1,	0x1.7df6fae65e424p-1,	// 0.596244, -log2l(0.596244)
											0x1.2fd9b8396ba9ep-1,	0x1.816cae7bd40b1p-1,	// 0.593458, -log2l(0.593458)
											0x1.2e6fecf2e6fedp-1,	0x1.84de4100b0ce2p-1,	// 0.590698, -log2l(0.590698)
											0x1.2d097b425ed09p-1,	0x1.884bbc446ae3fp-1,	// 0.587963, -log2l(0.587963)
											0x1.2ba6574cae996p-1,	0x1.8bb529f3ab8f3p-1,	// 0.585253, -log2l(0.585253)
											0x1.2a46756e62a46p-1,	0x1.8f1a9398f2d58p-1,	// 0.582569, -log2l(0.582569)
											0x1.28e9ca3a728eap-1,	0x1.927c029d3798ap-1,	// 0.579909, -log2l(0.579909)
											0x1.27904a7904a79p-1,	0x1.95d980488409ap-1,	// 0.577273, -log2l(0.577273)
											0x1.2639eb2639eb2p-1,	0x1.993315c28e8fbp-1,	// 0.574661, -log2l(0.574661)
											0x1.24e6a171024e7p-1,	0x1.9c88cc134f3c3p-1,	// 0.572072, -log2l(0.572072)
											0x1.239662b9f91cbp-1,	0x1.9fdaac2391e1cp-1,	// 0.569507, -log2l(0.569507)
											0x1.2249249249249p-1,	0x1.a328bebd84e8p-1,	// 0.566964, -log2l(0.566964)
											0x1.20fedcba98765p-1,	0x1.a6730c8d44efap-1,	// 0.564444, -log2l(0.564444)
											0x1.1fb78121fb781p-1,	0x1.a9b99e21655ebp-1,	// 0.561947, -log2l(0.561947)
											0x1.1e7307e4ef157p-1,	0x1.acfc7beb75e94p-1,	// 0.559471, -log2l(0.559471)
											0x1.1d31674c59d31p-1,	0x1.b03bae40852ap-1,	// 0.557018, -log2l(0.557018)
											0x1.1bf295cc93903p-1,	0x1.b3773d59a05ffp-1,	// 0.554585, -log2l(0.554585)
											0x1.1ab68a0473c1bp-1,	0x1.b6af315450638p-1,	// 0.552174, -log2l(0.552174)
											0x1.197d3abc65f4fp-1,	0x1.b9e3923313e58p-1,	// 0.549784, -log2l(0.549784)
											0x1.18469ee58469fp-1,	0x1.bd1467ddd70a7p-1,	// 0.547414, -log2l(0.547414)
											0x1.1712ad98b8957p-1,	0x1.c041ba2268731p-1,	// 0.545064, -log2l(0.545064)
											0x1.15e15e15e15e1p-1,	0x1.c36b90b4ebc3ap-1,	// 0.542735, -log2l(0.542735)
											0x1.14b2a7c2fee92p-1,	0x1.c691f33049bap-1,	// 0.540426, -log2l(0.540426)
											0x1.1386822b63cbfp-1,	0x1.c9b4e9169de22p-1,	// 0.538136, -log2l(0.538136)
											0x1.125ce4feeb7a1p-1,	0x1.ccd479d1a1f94p-1,	// 0.535865, -log2l(0.535865)
											0x1.1135c81135c81p-1,	0x1.cff0acb3170e3p-1,	// 0.533613, -log2l(0.533613)
											0x1.10112358e75d3p-1,	0x1.d30988f52c6d3p-1,	// 0.531381, -log2l(0.531381)
											0x1.0eeeeeeeeeeefp-1,	0x1.d61f15bae4663p-1,	// 0.529167, -log2l(0.529167)
											0x1.0dcf230dcf231p-1,	0x1.d9315a1076fa2p-1,	// 0.526971, -log2l(0.526971)
											0x1.0cb1b810ecf57p-1,	0x1.dc405cebb27dcp-1,	// 0.524793, -log2l(0.524793)
											0x1.0b96a673e2808p-1,	0x1.df4c252c5a3e1p-1,	// 0.522634, -log2l(0.522634)
											0x1.0a7de6d1d6086p-1,	0x1.e254b99c83339p-1,	// 0.520492, -log2l(0.520492)
											0x1.096771e4d528cp-1,	0x1.e55a20f0eecf9p-1,	// 0.518367, -log2l(0.518367)
											0x1.0853408534085p-1,	0x1.e85c61c963f0dp-1,	// 0.51626, -log2l(0.51626)
											0x1.07414ba8f0741p-1,	0x1.eb5b82b10609bp-1,	// 0.51417, -log2l(0.51417)
											0x1.06318c6318c63p-1,	0x1.ee578a1eaa83fp-1,	// 0.512097, -log2l(0.512097)
											0x1.0523fbe3367d7p-1,	0x1.f1507e752c6c8p-1,	// 0.51004, -log2l(0.51004)
											0x1.04189374bc6a8p-1,	0x1.f4466603be71dp-1,	// 0.508, -log2l(0.508)
											0x1.030f4c7e7859cp-1,	0x1.f73947063b3fdp-1,	// 0.505976, -log2l(0.505976)
											0x1.0208208208208p-1,	0x1.fa2927a574422p-1,	// 0.503968, -log2l(0.503968)
											0x1.0103091b51f5ep-1,	0x1.fd160df77ed7ap-1,	// 0.501976, -log2l(0.501976)
											0x1p-1,	0x1p+0,	// 0.5, -log2l(0.5)
										};


float powf( float x, float y )
{
	static const double recip_ln2 = 0x1.71547652b82fep0;

	if( x == 1.0f || y == 1.0f)
		return x;
		
	//Move the arguments to the integer registers for bitwise inspection
	union{ float f; uint32_t u; } ux, uy;
	ux.f = x;		
	uy.f = y;
	uint32_t absux = ux.u & 0x7fffffff;
	uint32_t absuy = uy.u & 0x7fffffff;

	
	// Handle most edge cases
	//If |x| or |y| is in { +-0, +-Inf, +-NaN } 
	if( (ux.u - 1U) >= 0x7f7fffff || (absuy - 1) >= 0x4affffff )
	{
		// any**0 = 1.0f for all values, including NaN
		if( 0 == absuy )
			return 1.0f;
	
		// handle NaNs
		if( x != x || y != y )
			return x + y;

		//figure out if y is an odd integer
		//Find out if y is an integer or not without raising inexact
		//	Note -- independently tested over entire range. Fails for Inf/NaN. We don't care about that here.
		uint32_t fractMask = 0x3fffffffU >> gMaskShift[ absuy >> 23 ];			//mask bits cover fractional part of value
		uint32_t onesMask = 0x40000000U >> gMaskShift[ absuy >> 23 ];			// we get away with this because leading exponent bit is never set for |y| < 2.0
		uint32_t fractionalBits = absuy & fractMask;
		uint32_t onesBit = absuy & onesMask;
			
		if( 0 == absux )
		{
			//if y is an odd integer
			if( 0 == fractionalBits && 0 != onesBit )	
			{
				if( y < 0.0f )
					return 1.0f / x;
			
				return x;
			}

			// y is not an odd integer
			if( 0.0f < y )
				return 0.0f;

			return 1.0f / __builtin_fabsf(x);			// return Inf and set div/0
		
		}
			
		// deal with infinite y
		if( 0x7f800000 == absuy )
		{
			if( -1.0f == x )
				return 1.0f;
		
			if( absux > 0x3f800000 )	// |x| > 1.0f
			{	// |x| > 1.0f
				if( 0.0f < y )
					return y;
				else
					return 0.0f;
			}
			else
			{	// |x| < 1.0f
				if( 0.0f < y )
					return 0.0f;
				else
					return __builtin_fabsf(y);
			}		
		}
	
		// we can also deal with x == +inf at this point.
		if( x == __builtin_inff() )
		{	
			if( y < 0.0f )
				return 0.0f;
			else
				return x;
		}
		
		if( x > -__builtin_inff() )
		{
			if( fractionalBits )
				goto nan_sqrt;
		
			goto ipowf;
		}
	
		// At this point, we know that x is in { +-0, -Inf } and y is finite non-zero.
		// Deal with y is odd integer cases
		if( 0 == fractionalBits && 0 != onesBit )	// if( |y| >= 1.0f || |y| < 0x1.0p24f )
			return 0.0f < y ? x : -0.0f; 

		// x == -inf		
		return 0.0f < y ? -x : 0.0f;
	}
	
	//special case for sqrts
	if( 0x3f000000U == absuy )
		goto nan_sqrt;
	
	// break |x| into exponent and fractional parts:		|x| = 2**i * m		1.0 <= m < 2.0
	int32_t	i = ((absux >> 23) & 0xff) - 127;
	union
	{
		uint32_t	u;
		float		f;
	}m = { (absux & 0x007fffffU) | 0x3f800000U };
	
	//normalize denormals
	if( -127 == i )
	{	//denormal
		m.f -= 1.0f;								//	exact
		i = ((m.u >> 23) & 0xff) - (127+126);
		m.u = (m.u & 0x807fffffU) | 0x3f800000U;
	}

	// 
	//	We further break down m as :
	//
	//          m = (1+a/256.0)(1+r)              a = high 8 explicit bits of mantissa(m), b = next 7 bits 
	//          log2f(m) = log2(1+a/256.0) + log2(1+r)
	//          
	//      We use the high 7 bits of the mantissa to look up log2(1+a/256.0) in log2f_table above
	//      We calculate 1+r as:
	//
	//          1+r = m * (1 /(1+a/256.0))
	//
	//      We can lookup (from the same table) the value of 1/(1+a/256.0) based on a too.

	double log2x = i;

	if( m.f != 1.0f )
	{
		int index = (m.u >> (23-7-4)) & 0x7f0;		//top 7 bits of mantissa
		const double *tablep = (void*) powf_log_table + index;
		double r = (double) m.f;

		// reduce
		r *= tablep[0];		// reduce r to  1-2**-7 < r < 1+2**-7
		log2x += tablep[1]; // do this early to force -1.0 + 1.0 to cancel so that we don't end up with (1.0 + tiny) - 1.0 later on.
		r -= 1.0;			// -2**-7 < r < 1+2**-7
		
		// ln(1+r) = r - rr/2 + rrr/3 - rrrr/4 + rrrrr/5
		//	should provide log(1+r) to at least 35 bits of accuracy for the worst case
		double rr = r*r;
		double small = -0.5 + 0.3333333333333333333333*r;
		double large = -0.25 + 0.2*r;
		double rrrr = rr * rr;
		small *= rr;
		small += r;
		large *= rrrr;
		r = small + large;
		log2x += r * recip_ln2;
	}

	// multiply by Y
	double ylog2x = y * log2x;

// now we need to calculate 2**ylog2x

	//deal with overflow
	if( ylog2x >= 128.0 )
		return (float) (0x1.0p128 * ylog2x);		//set overflow and return inf
	
	//deal with underflow
	if( ylog2x <= -150.0 )
		return (float) ( ylog2x * 0x1.0p-1022 );		//minimum y * maximum log2(x) is ~-1.0p128 * ~128 = -1.0p135, so we can be sure that we'll drive this to underflow
	
	//separate ylog2x into integer and fractional parts
	int exp = (int) ylog2x;
	double f = ylog2x - exp;		//may be negative
	
	// Calculate 2**fract
	// 8th order minimax fit of exp2 on [-1.0,1.0].  |error| < 0.402865722354948566583852e-9:
	static const double c0 =  1.0 + 0.278626872016317130037181614004e-10;
	static const double c1 = .693147176943623740308984004029708;
	static const double c2 = .240226505817268621584559118975830;
	static const double c3 = 0.555041568519883074165425891257052e-1;
	static const double c4overc8 = 0.961813690023115610862381719985771e-2 / 0.134107709538786543922336536865157e-5;
	static const double c5overc8 = 0.133318252930790403741964203236548e-2 / 0.134107709538786543922336536865157e-5;
	static const double c6overc8 = 0.154016177542147239746127455226575e-3 / 0.134107709538786543922336536865157e-5;
	static const double c7overc8 = 0.154832722143258821052933667742417e-4 / 0.134107709538786543922336536865157e-5;
	static const double c8 = 0.134107709538786543922336536865157e-5;

	double z = 1.0;
	if( 0.0 != f )
	{ // don't set inexact if we don't need to
		double ff = f * f;
		double s7 = c7overc8 * f;			double s3 = c3 * f;
		double s5 = c5overc8 * f;			double s1 = c1 * f;
		double ffff = ff * ff;
		s7 += c6overc8;						s3 += c2;
		s5 += c4overc8;						s1 += c0;
		s7 *= ff;							s3 *= ff;
		s5 += ffff;
		double c8ffff = ffff * c8;
		s7 += s5;							s3 += s1;
		s7 *= c8ffff;
		z = s3 + s7;
	}
		
	
	//prepare 2**i
	union{ uint64_t u; double d; } two_exp = { ((uint64_t) exp + 1023) << 52 };
	
	return (float) (z * two_exp.d );
	
	
	//one last edge case -- pow(x, y) returns NaN and raises invalid for x < 0 and finite non-integer y
	// and one special case --	call sqrt for |y| == 0.5
nan_sqrt:
	if( x < 0.0f || y > 0.0f )
		return sqrtf(x);

	return (float) sqrt( 1.0 / (double) x );
		
ipowf:
	// clamp  -0x1.0p31 < y < 0x1.0p31
	y = y > -0x1.fffffep30f ? y : -0x1.fffffep30f;
	y = y <  0x1.fffffep30f ? y :  0x1.fffffep30f;
	i = (int) y;
	double dx = (double) x;
	double r = 1.0;
	
	if( i < 0 )
	{
		i = -i;
		dx = 1.0 / dx;
	}
	
	if( i & 1 )
		r = dx;
	
	do
	{
		i >>= 1;
		if( 0 == i )
			break;
		dx *= dx;
		if( i & 1 )
			r *= dx;
	}while(1);

	return (float) r;
}

