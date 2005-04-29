inline void breakList(DString l,std::vector<DString> &out) {
	for(DString::size_type i=0,com = 0;i<l.size() && com<l.size();i = com+1) {
		while(isspace(l[i])) ++i;
		com = l.find(',',i);
		out.push_back(l.substr(i,com-i));
    }
}
