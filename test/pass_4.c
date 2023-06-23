bool bar(int x);

bool foo(int x) {
  if (x <= 0)
    return false;
  write x;
  return bar(x - 1);
}

bool bar(int x) {
  if (x <= 0)
    return false;
  write x;
  return foo(x - 1);
}

int main() { foo(5); }
