typedef struct list list;
typedef struct lroot lroot;

struct lroot {
	int count;
	struct list *first_node;
};

struct list {
  int fd;
  int id;
  char name[20];
  struct list *ptr;
};

struct lroot * init();
struct list * addelem(lroot *root, int fd, int id, char *name);
struct list  * deletelem(list *lst, lroot *root);
void listprint(lroot *root);
struct list * listfind(lroot *root, int id);
