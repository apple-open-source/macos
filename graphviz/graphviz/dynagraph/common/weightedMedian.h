/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

typedef std::vector<double> doubV;

template<typename T>
inline T weightedMedian(std::vector<T> &vec) {
	int n = vec.size();
	T m;
	switch(n) {
	case 0:	
		m = T(0.0); 
		abort(); 
		break;
	case 1: 
		m = vec[0]; 
		break;
	case 2: 
		m = (vec[0] + vec[1]) / 2.0; 
		break;
	default:	/* weighted median */
		std::sort(vec.begin(),vec.end());
		if(n % 2) 
			m = vec[n / 2];
		else {
			int rm = n / 2,
				lm = rm - 1;
			T rspan = vec[n - 1] - vec[rm],
				lspan = vec[lm] - vec[0];
			if(lspan == rspan)
				m = (vec[lm] + vec[rm]) / T(2.0);
			else {
				T w = vec[lm]*rspan + vec[rm]*lspan;
				m = w / (lspan + rspan);
			}
		}
		break;
	}
	return m;
}
