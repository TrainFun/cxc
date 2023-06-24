int main() {
  int x;
  for (int i = 0; i < 3; ++i) {
    read x;
    switch (x) {
    case 0:
      write 0;
    case 1:
    default:
      write 1;
    case 2: {
      write 2;
    }
    }
  }
}
