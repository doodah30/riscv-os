#include "kernel/types.h"
#include "user/user.h"

int main() {
  ps(); // 调用刚才写的系统调用
  exit(0);
}