; RUN: llvm-as < %s | llc -march=x86 -mattr=+mmx -o %t -f
; RUN: grep movq  %t

; Due to splatting the last element for widening <rdar://problem/7098635>,
; we would introduce a movq since a simple load/store would not suffice.

define void @foo(<2 x i16>* %A, <2 x i16>* %B) {
entry:
	%tmp1 = load <2 x i16>* %A		; <<2 x i16>> [#uses=1]
	store <2 x i16> %tmp1, <2 x i16>* %B
	ret void
}

