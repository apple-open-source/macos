class MyCls {
  public:
  inline MyCls::MyCls (int in) { float y = in; x = (int) y; }
  inline MyCls::MyCls (double in) { x = (int) in; }
  int x;
};


