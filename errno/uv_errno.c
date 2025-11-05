#include <stdlib.h>
#include <uv.h>

typedef struct uv_errno_pair_t {
  const char *name;
  uv_errno_t code;
  const char *msg;
} uv_errno_pair_t;

#define EXPAND_STRING(x) #x

static uv_errno_pair_t errs[] = {
#define XX(code, msg) {EXPAND_STRING(UV_##code), UV_##code, msg},
    UV_ERRNO_MAP(XX)
#undef XX
};

// NOTE 注意Windows上和Linux上，同样的错误码名字，其值不同！

int main(int argc, char **argv) {
  if (argc == 1) {
    for (int i = 0; i < sizeof(errs) / sizeof(uv_errno_pair_t); ++i) {
      printf("%s %d %s\n", errs[i].name, errs[i].code, errs[i].msg);
    }
    return 0;
  } else if (argc == 2) {
    int eno = atoi(argv[1]);
    printf("Name: %s, Msg: %s\n", uv_err_name(eno), uv_strerror(eno));
    return 0;
  } else {
    printf("Usage: uv_errno [errno]\n");
    return 1;
  }
}
