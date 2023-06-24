int main() {
  for (int i = 0;; ++i) {
    if (i < 10)
      continue;

    write i;

    if (i == 16)
      break;
  }
}
