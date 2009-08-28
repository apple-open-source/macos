#ifndef	_ARM_THREAD_STATUS_H_
#define _ARM_THREAD_STATUS_H_

#define ARM_THREAD_STATE        1

typedef struct arm_thread_state {
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	unsigned int r13;
	unsigned int r14;
	unsigned int r15;
	unsigned int r16;
} arm_thread_state_t;

#define ARM_THREAD_STATE_COUNT \
   (sizeof(struct arm_thread_state) / sizeof(int))

#endif /* _ARM_THREAD_STATUS_H_ */
