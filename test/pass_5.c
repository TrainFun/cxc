int gcd(int x, int y) {
  if (y == 0)
    return x;
  else
    return gcd(y, x % y);
}

int main() {
  int x;
  int y;
  read x;
  read y;
  write gcd(x, y);
}
