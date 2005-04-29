struct ReorientBadDir : DGException {
	ReorientBadDir() : DGException("bad direction argument to reorient()") {}
};
Coord reorient(Coord val,bool in,Orientation dir) {
	Coord ret;
	switch(dir) {
	case DG_ORIENT_UP:
		ret.x = -val.x;
		ret.y = -val.y;
		break;
	case DG_ORIENT_DOWN:
		ret.x = val.x;
		ret.y = val.y;
		break;
	case DG_ORIENT_LEFT:
		if(in)
			goto right;
	left:
		ret.x = val.y;
		ret.y = -val.x;
		break;
	case DG_ORIENT_RIGHT:
		if(in)
			goto left;
	right:
		ret.x = -val.y;
		ret.y = val.x;
		break;
	default:
		throw ReorientBadDir();
	}
	return ret;
}
