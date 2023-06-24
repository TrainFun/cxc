int pow(int x) {
  int ret = 1;
  while (x > 0) {
    ret = ret * x;
    --x;
  }

  return ret;
}

int main() {
  int x;
  read x;
  write pow(x);
}
