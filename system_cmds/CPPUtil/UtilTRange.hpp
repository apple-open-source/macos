//
//  UtilTRange.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/14/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_UtilTRange_hpp
#define CPPUtil_UtilTRange_hpp

template <typename T>
class TRange {
    protected:
	T	_location;
	T	_length;

    public:
	TRange() : _location(0), _length(0) {}
	TRange(T location, T length) : _location(location), _length(length) {
		DEBUG_ONLY(validate());
	};

	bool operator==(const TRange &rhs) const 		{ return this->_location == rhs.location() && this->_length == rhs.length(); }

	bool operator!=(const TRange &rhs) const 		{ return !(*this == rhs); }

	bool operator<(const TRange& rhs) const			{ return this->_location < rhs.location(); }
	bool operator<(const TRange* rhs) const			{ return this->_location < rhs->location(); }

	const T location() const				{ return _location; }
	const T length() const					{ return _length; }
	const T max() const					{ return _location + _length; }

	void set_location(T location)				{ _location = location; DEBUG_ONLY(validate()); }
	void set_length(T length)				{ _length = length; DEBUG_ONLY(validate()); }
	void set_max(T max)					{ ASSERT(max >= _location, "Sanity"); _length = max - _location; }
	
	const bool contains(const TRange& other) const		{ return (other.location() >= location()) && (other.max() <= max()); }
	const bool contains(const T loc) const			{ return loc - _location < _length; } // Assumes unsigned!

	const bool intersects(const TRange& o) const		{ return this->location() < o.max() && o.location() < this->max(); }

	// "union" is a keyword :-(
	TRange union_range(const TRange& other) const {
		T maxend = (this->max() > other.max()) ? this->max() : other.max();
		T minloc = this->location() < other.location() ? this->location() : other.location();
		return TRange(minloc, maxend - minloc);
	}

	TRange intersection_range(const TRange& other) const {
		if (this->intersects(other)) {
			auto intersection_start = std::max(_location, other.location());
			auto intersection_end = std::min(max(), other.max());
			return TRange(intersection_start, intersection_end - intersection_start);
		}

		return TRange(T(0),T(0));
	}

	void validate() const					{ ASSERT((_location + _length >= _location) /*|| (_location + 1 == 0)*/, "range must not wrap"); }
};

template <typename TRANGE>
bool is_trange_vector_sorted_and_non_overlapping(const std::vector<TRANGE>& vec) {
	if (vec.size() > 1) {
		auto last_it = vec.begin();
		auto it = last_it + 1;

		while (it < vec.end()) {
			if (it < last_it)
				return false;
			
			if (last_it->intersects(*it))
				return false;
			
			last_it = it;
			it++;
		}
	}
	return true;
}

template <typename TRANGE>
bool is_trange_vector_sorted(const std::vector<TRANGE>& vec) {
	if (vec.size() > 1) {
		auto last_it = vec.begin();
		auto it = last_it + 1;

		while (it < vec.end()) {
			if (it < last_it)
				return false;

			last_it = it;
			it++;
		}
	}
	return true;
}

// NOTE!
//
// This produces an output vector with the
// intervals "flattened".
//
// IOW, this:
//
// vec1: XXXXXXXX       AAAAAAAAAA
//        YYYYYYYYYYY ZZZZZZZZZ
//
// becomes:
//
// res:  IIIIIIIIIIII IIIIIIIIIIII
//
// The input vector should be sorted.
//
template <typename TRANGE>
std::vector<TRANGE> trange_vector_union(std::vector<TRANGE>& input) {
	std::vector<TRANGE> union_vec;

	ASSERT(is_trange_vector_sorted(input), "Sanity");

	if (!input.empty()) {
		auto input_it = input.begin();
		union_vec.push_back(*input_it);
		while (++input_it < input.end()) {
			TRANGE union_range = union_vec.back();

			if (union_range.intersects(*input_it)) {
				union_vec.pop_back();
				union_vec.push_back(union_range.union_range(*input_it));
			} else {
				ASSERT(union_range < *input_it, "Out of order merging");
				union_vec.push_back(*input_it);
			}
		}
	}

	ASSERT(is_trange_vector_sorted_and_non_overlapping(union_vec), "union'd vector fails invariant");

	return union_vec;
}

// NOTE!
//
// This will coalesce intervals that intersect.
//
// IOW, given two input vectors:
//
// vec1:      XXXX           XXXX
// vec2:              XXX
//
// res:       XXXX    XXX    XXXX
//
// --------------------------------
//
// vec1:      XXXX         XX
// vec2:        XXXXXXXXXXXXXXX
//
// res:       XXXXXXXXXXXXXXXXX

template <typename TRANGE>
std::vector<TRANGE> trange_vector_union(std::vector<TRANGE>& vec1, std::vector<TRANGE>& vec2) {
	std::vector<TRANGE> union_vec;

	ASSERT(is_trange_vector_sorted_and_non_overlapping(vec1), "input vector violates invariants");
	ASSERT(is_trange_vector_sorted_and_non_overlapping(vec2), "input vector violates invariants");

	// while (not done)
	//     select next interval (lowest location)
	//     if intersects with last union_vec entry, union, pop_back, push_back
	//     else push_back

	auto vec1_it = vec1.begin();
	auto vec2_it = vec2.begin();

	while (uint32_t chose_vector = (((vec1_it != vec1.end()) ? 1 : 0) + ((vec2_it != vec2.end()) ? 2 : 0))) {
		//
		// This is a fancy "chose" algorithm
		//
		// vec1 == bit 1
		// vec2 == bit 2
		//
		decltype(vec1_it) merge_it;
		switch (chose_vector) {
			case 1:
				merge_it = vec1_it++;
				break;

			case 2:
				merge_it = vec2_it++;
				break;

			case 3:
				merge_it = (*vec1_it < * vec2_it) ? vec1_it++ : vec2_it++;
				break;

			default:
				ASSERT(false, "ShouldNotReachHere");
				return std::vector<TRANGE>();
		}

		if (union_vec.empty()) {
			union_vec.push_back(*merge_it);
		} else {
			TRANGE last_range = union_vec.back();

			if (last_range.intersects(*merge_it)) {
				union_vec.pop_back();
				union_vec.push_back(last_range.union_range(*merge_it));
			} else {
				ASSERT(last_range < *merge_it, "Out of order merging");
				union_vec.push_back(*merge_it);
			}
		}
	}

	ASSERT(is_trange_vector_sorted_and_non_overlapping(union_vec), "union'd vector fails invariant");

	return union_vec;
}

template <typename TRANGE>
std::vector<TRANGE> trange_vector_intersect(std::vector<TRANGE>& vec1, std::vector<TRANGE>& vec2) {
	std::vector<TRANGE> intersect_vec;

	ASSERT(is_trange_vector_sorted_and_non_overlapping(vec1), "input vector violates invariants");
	ASSERT(is_trange_vector_sorted_and_non_overlapping(vec2), "input vector violates invariants");

	auto vec1_it = vec1.begin();
	auto vec2_it = vec2.begin();

	// As soon as one vector empties, there can be no more intersections
	while (vec1_it != vec1.end() && vec2_it  != vec2.end()) {
		TRANGE temp = vec1_it->intersection_range(*vec2_it);
		if (temp.length() > 0) {
			intersect_vec.push_back(temp);
		}

		// We keep the interval that ends last

		if (vec1_it->max() > vec2_it->max()) {
			vec2_it++;
		} else {
			vec1_it++;
		}
	}

	ASSERT(is_trange_vector_sorted_and_non_overlapping(intersect_vec), "intersection vector fails invariant");

	return intersect_vec;
}

#endif
