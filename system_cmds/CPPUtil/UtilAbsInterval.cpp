//
//  UtilAbsInterval.cpp
//  CPPUtil
//
//  Created by James McIlree on 9/8/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

const AbsInterval* interval_beginning_timespan(const std::vector<AbsInterval>& intervals, AbsInterval timespan) {
	auto it = std::upper_bound(intervals.begin(), intervals.end(), timespan.location(), AbsIntervalMaxVsAbsTimeComparator());

	//
	// For a beginning interval, there is no possible match if timespan.location() > intervals.back().max()
	//
	if (it != intervals.end()) {
		//
		// We found something. Does it contain the search point?
		//
		if (it->contains(timespan.location())) {
			return &*it;
		}

		//
		// If the AbsInterval found intersects the timespan, its still the first valid vm_fault in
		// the given timespan, so return it anyway.
		//
		if (it->intersects(timespan)) {
			return &*it;
		}
	}

	return NULL;
}

const AbsInterval* interval_ending_timespan(const std::vector<AbsInterval>& intervals, AbsInterval timespan) {

	// We could do this as timespan.max() and use lower_bound(...) to save the subtraction.
	// But we need the max()-1 value later for the contains() test anyway, so might as well calculate
	// it here.
	AbsTime max = timespan.max() - AbsTime(1);
	auto it = std::upper_bound(intervals.begin(), intervals.end(), max, AbsIntervalMaxVsAbsTimeComparator());

	// Did we find something?
	if (it != intervals.end()) {

		if (it->contains(max)) {
			return &*it;
		}

		// Okay, the matched interval is to the "right" of us on the
		// timeline. Is there a previous interval that might work?
		if (it != intervals.begin()) {
			if ((--it)->intersects(timespan)) {
				return &*it;
			}
		}
	} else {
		// Okay, we're off the end of the timeline. There still might
		// be a previous interval that would match.
		if (!intervals.empty()) {
			if ((--it)->intersects(timespan)) {
				return &*it;
			}
		}
	}

	return NULL;
}

END_UTIL_NAMESPACE
