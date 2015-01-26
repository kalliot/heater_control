
#ifndef __LISTS__
#define __LISTS__
          

struct endpoints {
   struct Node *lh_Head;
   struct Node *lh_Tail;
   struct Node *lh_TailPred;
};


struct Node {
   struct Node *ln_Succ;
   struct Node *ln_Pred;
   long   ln_Pri;
   char   *ln_Name;
};

class List {
 public:
  List(void);
  void           addhead   (struct Node *);
  void           addtail   (struct Node *);
  void           enqueue   (struct Node *);
  void           addsorted (struct Node *);
  void           insert    (struct Node *,struct Node *);
  void           remove    (struct Node *);
  struct Node   *remhead   (void);
  struct Node   *remtail   (void);
  struct Node   *findname  (struct Node *,char *name);
  struct Node   *getHead   (void);
  struct Node   *getTail   (void);
  struct Node   *iterForward   (struct Node *n,int (* cb)(struct Node *n,void *data), void *data);
  struct Node   *iterBack      (struct Node *n,int (* cb)(struct Node *n,void *data), void *data);
 private:
  struct endpoints _entry;
};

#endif
