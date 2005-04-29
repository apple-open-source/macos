#pragma once

template <class T>
class StPointer {
	public:
		StPointer (T*	inPointer):
			mPointer (inPointer) {
			CCIAssert_ (CCIValidPointer (mPointer));
		}
		
		StPointer& operator = (T	inRhs) {
			*mPointer = inRhs;
			return *this;
		}
		
	private:
		T*	mPointer;
};

