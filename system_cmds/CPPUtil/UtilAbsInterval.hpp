//
//  UtilAbsInterval.h
//  CPPUtil
//
//  Created by James McIlree on 4/14/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __CPPUtil__UtilAbsInterval__
#define __CPPUtil__UtilAbsInterval__

typedef TRange<AbsTime> AbsInterval;

struct AbsIntervalLocationVsAbsTimeComparator {
	bool operator()(const AbsInterval& activity, const AbsTime& time) const {
		return activity.location() < time;
	}

	bool operator()(const AbsTime& time, const AbsInterval& activity) const {
		return time < activity.location();
	}
};

struct AbsIntervalMaxVsAbsTimeComparator {
	bool operator()(const AbsInterval& activity, const AbsTime& time) const {
		return activity.max() < time;
	}

	bool operator()(const AbsTime& time, const AbsInterval& activity) const {
		return time < activity.max();
	}
};

//
// Takes a vector of sorted non overlapping AbsInterval(s), and a timespan. Returns a pointer to the
// youngest AbsInterval that intersects the timespan. Returns NULL if no interval intersects.
//
// vec: XXXXXXX XXXXXXXX XXXXX XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret: XXXXXXX
//
// ----------------------------------
//
// vec:         XXXXXXXX XXXXX XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret:         XXXXXXXX
//
// ----------------------------------
//
// vec:                  XXXXX XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret:                  XXXXX
//
// ----------------------------------
//
// vec:                        XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret: NULL
//

const AbsInterval* interval_beginning_timespan(const std::vector<AbsInterval>& intervals, AbsInterval timespan);

//
// Takes a vector of sorted non overlapping AbsInterval(s), and a timespan. Returns a pointer to the
// oldest AbsInterval that intersects the timespan. Returns NULL if no interval intersects.
//
// vec: XXXXXXX XXXXXXXX XXXXX XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret: XXXXXXX
//
// ----------------------------------
//
// vec:         XXXXXXXX XXXXX XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret:         XXXXXXXX
//
// ----------------------------------
//
// vec:                  XXXXX XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret:                  XXXXX
//
// ----------------------------------
//
// vec:                        XXXXXX
//  ts:      MMMMMMMMMMMMMMM
// ret: NULL
//

const AbsInterval* interval_ending_timespan(const std::vector<AbsInterval>& intervals, AbsInterval timespan);

#endif /* defined(__CPPUtil__UtilAbsInterval__) */
