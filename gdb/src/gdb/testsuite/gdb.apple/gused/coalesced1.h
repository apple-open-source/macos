class MyCls {
  public:
  inline MyCls (int in) { float y = in; x = (int) y; }
  inline MyCls (double in) { x = (int) in; }
  int x;
};


