int main() {
  int x = 5;
  for (;;) {
    write x;
    --x;
    if (x <= 0)
      return 0;
  }
}
