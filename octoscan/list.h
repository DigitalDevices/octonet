struct list_head {
	struct list_head *prev;
	struct list_head *next;
};

#define list_entry(lh, st, field) container_of(lh, st, field)
#define list_first_entry(lh, st, field) list_entry((lh)->next, st, field)
#define list_next_entry(lh, field) list_entry((lh)->field.next, typeof(*(lh)), field)

static inline void list_head_init(struct list_head *lh)
{
	lh->prev = lh->next = lh;
}

static inline int list_empty(const struct list_head *lh)
{
	return (lh == lh->next);
}

static inline void list_add(struct list_head *add, struct list_head *lh)
{
	add->prev = lh;
	add->next = lh->next;
	lh->next->prev = add;
	lh->next = add;
}

static inline void list_add_tail(struct list_head *add, struct list_head *lh)
{
	add->prev = lh->prev;
	add->next = lh;
	lh->prev->next = add;
	lh->prev = add;
}

static inline void list_del(struct list_head *del)
{
	del->prev->next = del->next;
	del->next->prev = del->prev;
	del->prev = del->next = NULL;
}

#define list_for_each_entry(cur, lh, field)				\
	for ((cur) = list_entry((lh)->next, typeof(*(cur)), field);	\
	     &(cur)->field != (lh);					\
	     (cur) = list_entry((cur)->field.next, typeof(*(cur)), field))

#define list_for_each_entry_safe(cur, nxt, lh, field)			\
	for ((cur) = list_entry((lh)->next, typeof(*(cur)), field),	\
		     (nxt) = list_next_entry((cur), field);		\
	     &(cur)->field != (lh);					\
	     (cur) = (nxt), (nxt) = list_next_entry((nxt), field))

