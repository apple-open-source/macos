struct xx { int x; int y; };

struct xx f () {
  struct xx x = { 3, 4 };
  return x;
}

int main () {
  struct xx l = f ();
}

