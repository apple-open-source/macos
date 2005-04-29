/* gcc-3.3 can't actually compile the source to this file, so it was built
   using GCC 4.0 and supplied here in assembler.  */

	.stabs	"/",100,0,2,Ltext0
	.stabs	"darwin-ldouble.c",100,0,2,Ltext0
.text
Ltext0:
	.stabs	"gcc2_compiled.",60,0,0,0
.data
.literal8
	.align 3
LC0:
	.long	2146435072
	.long	0
	.align 3
LC1:
	.long	0
	.long	0
.text
	.align 2
	.globl __xlqadd
__xlqadd:
	.stabd	68,0,79
	mflr r0
	bcl 20,31,"L00000000001$pb"
"L00000000001$pb":
	stw r31,-4(r1)
	.stabd	68,0,83
	fadd f12,f1,f3
	.stabd	68,0,79
	mflr r31
	stw r0,8(r1)
	.stabd	68,0,85
	fabs f0,f12
	addis r2,r31,ha16(LC0-"L00000000001$pb")
	lfd f11,lo16(LC0-"L00000000001$pb")(r2)
	fcmpu cr7,f0,f11
	bnl- cr7,L18
	.stabd	68,0,99
	fsub f0,f1,f12
	.stabd	68,0,100
	fadd f13,f12,f0
	fadd f0,f3,f0
	fsub f13,f1,f13
	fadd f0,f0,f13
	fadd f0,f0,f2
	fadd f2,f0,f4
	.stabd	68,0,101
	fadd f1,f12,f2
	.stabd	68,0,103
	fabs f0,f1
	fcmpu cr7,f0,f11
	bnl- cr7,L19
	.stabd	68,0,107
	fsub f0,f12,f1
	.stabd	68,0,106
	stfd f1,-48(r1)
	.stabd	68,0,107
	fadd f0,f2,f0
	stfd f0,-40(r1)
L12:
	.stabd	68,0,109
	lfd f1,-48(r1)
	lfd f2,-40(r1)
L8:
	.stabd	68,0,110
	lwz r0,8(r1)
	lwz r31,-4(r1)
	mtlr r0
	blr
L18:
	.stabd	68,0,87
	fadd f2,f2,f4
	fadd f0,f3,f2
	fadd f12,f1,f0
	.stabd	68,0,88
	fabs f13,f12
	fcmpu cr7,f13,f11
	bnl- cr7,L20
	.stabd	68,0,92
	fabs f0,f1
	.stabd	68,0,90
	stfd f12,-48(r1)
	.stabd	68,0,92
	fabs f13,f3
	fcmpu cr7,f0,f13
	bng- cr7,L9
	.stabd	68,0,93
	fsub f0,f1,f12
	fadd f0,f3,f0
	fadd f0,f2,f0
	stfd f0,-40(r1)
	b L12
L19:
	.stabd	68,0,104
	addis r2,r31,ha16(LC1-"L00000000001$pb")
	lfd f2,lo16(LC1-"L00000000001$pb")(r2)
	b L8
L9:
	.stabd	68,0,95
	fsub f0,f3,f12
	fadd f0,f1,f0
	fadd f0,f2,f0
	stfd f0,-40(r1)
	b L12
L20:
	.stabd	68,0,89
	addis r2,r31,ha16(LC1-"L00000000001$pb")
	fmr f1,f12
	lfd f2,lo16(LC1-"L00000000001$pb")(r2)
	b L8
	.stabs	"_xlqadd:F(0,1)=r(0,0);16;0;",36,0,79,__xlqadd
	.stabs	"long double:t(0,1)",128,0,0,0
	.stabs	"a:P(0,2)=r(0,0);8;0;",64,0,78,33
	.stabs	"aa:P(0,2)",64,0,78,34
	.stabs	"c:P(0,2)",64,0,78,35
	.stabs	"cc:P(0,2)",64,0,78,36
	.stabs	"double:t(0,2)",128,0,0,0
	.stabs	"x:(0,3)=(0,4)=u16ldval:(0,1),0,128;dval:(0,5)=ar(0,6)=r(0,6);000000000000000000000000;000000000000037777777777;;0;1;(0,2),0,128;;",128,0,80,-48
	.stabs	"longDblUnion:t(0,3)",128,0,74,0
	.stabs	"long unsigned int:t(0,7)=r(0,7);000000000000000000000000;000000000000037777777777;",128,0,0,0
	.stabs	"z:r(0,2)",64,0,81,44
	.stabs	"q:r(0,2)",64,0,81,32
	.stabs	"zz:r(0,2)",64,0,81,34
	.stabs	"xh:r(0,2)",64,0,81,33
	.stabn	192,0,0,__xlqadd
	.stabn	224,0,0,Lscope0
Lscope0:
	.stabs	"",36,0,0,Lscope0-__xlqadd
	.align 2
	.globl __xlqsub
__xlqsub:
	.stabd	68,0,114
	stw r31,-4(r1)
	.stabd	68,0,115
	fneg f3,f3
	fneg f4,f4
	.stabd	68,0,116
	lwz r31,-4(r1)
	.stabd	68,0,115
	b __xlqadd
	.stabs	"_xlqsub:F(0,1)",36,0,114,__xlqsub
	.stabs	"a:P(0,2)",64,0,113,33
	.stabs	"b:P(0,2)",64,0,113,34
	.stabs	"c:P(0,2)",64,0,113,35
	.stabs	"d:P(0,2)",64,0,113,36
Lscope1:
	.stabs	"",36,0,0,Lscope1-__xlqsub
.data
.literal8
	.align 3
LC2:
	.long	2146435072
	.long	0
	.align 3
LC3:
	.long	0
	.long	0
.text
	.align 2
	.globl __xlqmul
__xlqmul:
	.stabd	68,0,120
	mflr r0
	bcl 20,31,"L00000000002$pb"
"L00000000002$pb":
	stw r31,-4(r1)
	fmr f12,f1
	mflr r31
	.stabd	68,0,124
	fmul f1,f1,f3
	.stabd	68,0,120
	stw r0,8(r1)
	.stabd	68,0,126
	addis r2,r31,ha16(LC2-"L00000000002$pb")
	fabs f13,f1
	lfd f11,lo16(LC2-"L00000000002$pb")(r2)
	addis r2,r31,ha16(LC3-"L00000000002$pb")
	lfd f0,lo16(LC3-"L00000000002$pb")(r2)
	fcmpu cr7,f1,f0
	beq- cr7,L24
	fcmpu cr7,f13,f11
	bnl- cr7,L24
	.stabd	68,0,136
	fmul f0,f3,f2
	.stabd	68,0,133
	fmsub f13,f12,f3,f1
	.stabd	68,0,136
	fmadd f0,f12,f4,f0
	fadd f2,f13,f0
	.stabd	68,0,137
	fadd f12,f1,f2
	.stabd	68,0,140
	fabs f0,f12
	fcmpu cr7,f0,f11
	bnl- cr7,L33
	.stabd	68,0,143
	fsub f0,f1,f12
	.stabd	68,0,142
	stfd f12,-48(r1)
	.stabd	68,0,144
	lfd f1,-48(r1)
	.stabd	68,0,143
	fadd f0,f2,f0
	stfd f0,-40(r1)
	.stabd	68,0,144
	lfd f2,-40(r1)
L28:
	.stabd	68,0,145
	lwz r0,8(r1)
	lwz r31,-4(r1)
	mtlr r0
	blr
L24:
	lwz r0,8(r1)
	.stabd	68,0,128
	addis r2,r31,ha16(LC3-"L00000000002$pb")
	lfd f2,lo16(LC3-"L00000000002$pb")(r2)
	.stabd	68,0,145
	mtlr r0
	lwz r31,-4(r1)
	blr
L33:
	.stabd	68,0,141
	addis r2,r31,ha16(LC3-"L00000000002$pb")
	fmr f1,f12
	lfd f2,lo16(LC3-"L00000000002$pb")(r2)
	b L28
	.stabs	"_xlqmul:F(0,1)",36,0,120,__xlqmul
	.stabs	"a:P(0,2)",64,0,119,44
	.stabs	"b:P(0,2)",64,0,119,34
	.stabs	"c:P(0,2)",64,0,119,35
	.stabs	"d:P(0,2)",64,0,119,36
	.stabs	"z:(0,3)",128,0,121,-48
	.stabs	"t:r(0,2)",64,0,122,33
	.stabs	"tau:r(0,2)",64,0,122,45
	.stabs	"u:r(0,2)",64,0,122,44
	.stabn	192,0,0,__xlqmul
	.stabn	224,0,0,Lscope2
Lscope2:
	.stabs	"",36,0,0,Lscope2-__xlqmul
.data
.literal8
	.align 3
LC4:
	.long	2146435072
	.long	0
	.align 3
LC5:
	.long	0
	.long	0
.text
	.align 2
	.globl __xlqdiv
__xlqdiv:
	.stabd	68,0,149
	fmr f11,f1
	mflr r0
	.stabd	68,0,153
	fdiv f1,f1,f3
	.stabd	68,0,149
	bcl 20,31,"L00000000003$pb"
"L00000000003$pb":
	stw r31,-4(r1)
	mflr r31
	stw r0,8(r1)
	.stabd	68,0,155
	addis r2,r31,ha16(LC4-"L00000000003$pb")
	lfd f10,lo16(LC4-"L00000000003$pb")(r2)
	addis r2,r31,ha16(LC5-"L00000000003$pb")
	lfd f0,lo16(LC5-"L00000000003$pb")(r2)
	fcmpu cr7,f1,f0
	fabs f13,f1
	beq- cr7,L35
	fcmpu cr7,f13,f10
	bnl- cr7,L35
	.stabd	68,0,161
	fmul f0,f3,f1
	.stabd	68,0,166
	fmsub f12,f3,f1,f0
	.stabd	68,0,169
	fnmsub f13,f1,f4,f2
	fsub f0,f11,f0
	fsub f0,f0,f12
	fadd f13,f13,f0
	fdiv f2,f13,f3
	.stabd	68,0,170
	fadd f12,f1,f2
	.stabd	68,0,173
	fabs f0,f12
	fcmpu cr7,f0,f10
	bnl- cr7,L44
	.stabd	68,0,176
	fsub f0,f1,f12
	.stabd	68,0,175
	stfd f12,-48(r1)
	.stabd	68,0,177
	lfd f1,-48(r1)
	.stabd	68,0,176
	fadd f0,f2,f0
	stfd f0,-40(r1)
	.stabd	68,0,177
	lfd f2,-40(r1)
L39:
	.stabd	68,0,178
	lwz r0,8(r1)
	lwz r31,-4(r1)
	mtlr r0
	blr
L35:
	lwz r0,8(r1)
	.stabd	68,0,157
	addis r2,r31,ha16(LC5-"L00000000003$pb")
	lfd f2,lo16(LC5-"L00000000003$pb")(r2)
	.stabd	68,0,178
	mtlr r0
	lwz r31,-4(r1)
	blr
L44:
	.stabd	68,0,174
	addis r2,r31,ha16(LC5-"L00000000003$pb")
	fmr f1,f12
	lfd f2,lo16(LC5-"L00000000003$pb")(r2)
	b L39
	.stabs	"_xlqdiv:F(0,1)",36,0,149,__xlqdiv
	.stabs	"a:P(0,2)",64,0,148,43
	.stabs	"b:P(0,2)",64,0,148,34
	.stabs	"c:P(0,2)",64,0,148,35
	.stabs	"d:P(0,2)",64,0,148,36
	.stabs	"z:(0,3)",128,0,150,-48
	.stabs	"s:r(0,2)",64,0,151,32
	.stabs	"sigma:r(0,2)",64,0,151,44
	.stabs	"t:r(0,2)",64,0,151,33
	.stabs	"tau:r(0,2)",64,0,151,34
	.stabs	"u:r(0,2)",64,0,151,44
	.stabn	192,0,0,__xlqdiv
	.stabn	224,0,0,Lscope3
Lscope3:
	.stabs	"",36,0,0,Lscope3-__xlqdiv
	.stabs "",100,0,0,Letext
Letext:
	.subsections_via_symbols
