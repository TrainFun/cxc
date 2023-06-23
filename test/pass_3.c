int x = 123;

int main() {
  write x;
  {
    int x = 234;
    x = 345;
    write x;
  }
  write x;
}
