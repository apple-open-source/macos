//
//  MachineCPU.impl.hpp
//  KDBG
//
//  Created by James McIlree on 11/7/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

//
// NOTE! activity match behavior explanation...
//
// CPUActivity entries are contiguous, there are no holes in the timeline.
//
// Note the operator< definitions above, std::lower_bounds is not using the
// default AbsInterval <. The comparsions are against the interval(s) max() - 1.
//
// std::lower_bound returns a match doing <=, std::upper_bound returns a match doing <
//
// 8/26/13...
//
// Okay, based on a better understanding of the behavior of xxx_bounds, this
// should be switchable to std::upper_bounds using a comparator without the
// subtraction, and so slightly more efficient.
//

template <typename SIZE>
const CPUActivity<SIZE>* MachineCPU<SIZE>::activity_for_timestamp(AbsTime timestamp) const {
	auto it = std::upper_bound(_timeline.begin(), _timeline.end(), timestamp, AbsIntervalMaxVsAbsTimeComparator());

	// The upper bound will report that 0 is lower than [ 10, 20 ), need to check contains!
	if (it != _timeline.end() && it->contains(timestamp)) {
		return &*it;
	}

	return NULL;
}
