#include <Arduino.h>
#include <stdio.h>   /* NULL definition */
#include <string.h>
#include "lists.h"


List::List(void)
{
  _entry.lh_Head     = (struct Node *)&_entry.lh_Tail;
  _entry.lh_TailPred = (struct Node *)&_entry.lh_Head;
  _entry.lh_Tail     = NULL;
}

void List::addhead(struct Node *n)
{
  insert(n, NULL);
}

void List::addtail(struct Node *n)
{
  insert(n, _entry.lh_TailPred);
}

void List::enqueue(struct Node *node)
{
  struct Node *search;

  for (search=_entry.lh_TailPred;search->ln_Pred;search=search->ln_Pred) {
    if (search->ln_Pri > node->ln_Pri) {
      insert(node,search);
      return;
    }
  }
  addhead(node);
}

void List::addsorted(struct Node *node)
{
  struct Node *search;

  for (search=_entry.lh_TailPred;search->ln_Pred != NULL;search=search->ln_Pred) {
    if (strcmp(search->ln_Name,node->ln_Name) < 0) {
      insert(node,search);
      return;
    }
  }
  addhead(node);
}

void List::insert(struct Node *n,struct Node *prev)
{
  if (!prev)
    prev = (struct Node *)&_entry.lh_Head;
  n->ln_Succ = prev->ln_Succ;
  n->ln_Pred = prev;
  prev->ln_Succ = n;
  n->ln_Succ->ln_Pred = n;
}

void List::remove(struct Node *node)
{
  node->ln_Pred->ln_Succ=node->ln_Succ;
  node->ln_Succ->ln_Pred=node->ln_Pred;
}

struct Node *List::getHead(void)
{
  return _entry.lh_Head;
}

struct Node *List::getTail(void)
{
  return _entry.lh_TailPred;
}

struct Node *List::remhead(void)
{
  struct Node *n = _entry.lh_Head;

  if (n->ln_Succ) {
    remove(n);
    return(n);
  }
  return(NULL);
}


struct Node *List::remtail(void)
{
  struct Node *a = _entry.lh_TailPred;

  if (a->ln_Pred) {
    remove(a);
    return(a);
  }
  return(NULL);
}


struct Node *List::findname(struct Node *n,char *name)
{
  struct Node *search;

  for (search=n;search->ln_Succ;search=search->ln_Succ) {
    if ((search->ln_Name) && !strcmp(name,search->ln_Name))
      return (search);
  }
  return NULL;
}

struct Node *List::iterForward(struct Node *n,int (* cb)(struct Node *n,void *data), void *data)
{
  struct Node *search;
  int cbret;

  for (search=n;search->ln_Succ;search=search->ln_Succ) {
    if (cb(search,data))
      return search;
  }
  return NULL;
}

struct Node *List::iterBack(struct Node *n,int (* cb)(struct Node *n,void *data), void *data)
{
  struct Node *search;
  int cbret;

  for (search=n;search->ln_Pred;search=search->ln_Pred) {
    if (cb(search,data))
      return search;
  }
  return NULL;
}

