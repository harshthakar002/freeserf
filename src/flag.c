/* flag.c */

#include <stdio.h>
#include <stdlib.h>

#include "flag.h"
#include "building.h"
#include "player.h"
#include "list.h"
#include "misc.h"

#define SEARCH_MAX_DEPTH  0x10000


typedef struct {
	list_elm_t elm;
	flag_t *flag;
} flag_proxy_t;


static flag_proxy_t *
flag_proxy_alloc(flag_t *flag)
{
	flag_proxy_t *proxy = malloc(sizeof(flag_proxy_t));
	if (proxy == NULL) abort();

	proxy->flag = flag;
	return proxy;
}

void
flag_search_init(flag_search_t *search)
{
	list_init(&search->queue);
	search->id = init_flag_search();
}

void
flag_search_add_source(flag_search_t *search, flag_t *flag)
{
	flag_proxy_t *flag_proxy = flag_proxy_alloc(flag);
	list_append(&search->queue, (list_elm_t *)flag_proxy);
	flag->search_num = search->id;
}

int
flag_search_execute(flag_search_t *search, flag_search_func *callback, int land, int transporter, void *data)
{
	for (int i = 0; i < SEARCH_MAX_DEPTH && !list_is_empty(&search->queue); i++) {
		flag_proxy_t *proxy = (flag_proxy_t *)list_remove_head(&search->queue);
		flag_t *flag = proxy->flag;
		free(proxy);

		int r = callback(flag, data);
		if (r) {
			/* Clean up */
			while (!list_is_empty(&search->queue)) {
				free(list_remove_head(&search->queue));
			}
			return 0;
		}

		for (int i = 0; i < 6; i++) {
			if ((!land || BIT_TEST(flag->endpoint, 5-i)) && /* Across land */
			    (!transporter || BIT_TEST(flag->transporter, 5-i)) &&
			    flag->other_endpoint.f[5-i]->search_num != search->id) {
				flag->other_endpoint.f[5-i]->search_num = search->id;
				flag->other_endpoint.f[5-i]->search_dir = flag->search_dir;
				flag_proxy_t *other_flag_proxy = flag_proxy_alloc(flag->other_endpoint.f[5-i]);
				list_append(&search->queue, (list_elm_t *)other_flag_proxy);
			}
		}
	}

	/* Clean up */
	while (!list_is_empty(&search->queue)) {
		free(list_remove_head(&search->queue));
	}

	return -1;
}

int
flag_search_single(flag_t *src, flag_search_func *callback,
		   int land, int transporter, void *data)
{
	flag_search_t search;
	flag_search_init(&search);
	flag_search_add_source(&search, src);
	return flag_search_execute(&search, callback, land, transporter, data);
}

void
flag_prioritize_pickup(flag_t *flag, dir_t dir, const int flag_prio[])
{
	int res_next = -1;
	int res_prio = -1;

	for (int i = 0; i < 7; i++) {
		/* Use flag_prio to prioritize resource pickup. */
		dir_t res_dir = ((flag->res_waiting[i] >> 5) & 7)-1;
		resource_type_t res_type = (flag->res_waiting[i] & 0x1f)-1;
		if (res_dir == dir && flag_prio[res_type] > res_prio) {
			res_next = i;
			res_prio = flag_prio[res_type];
		}
	}

	flag->other_end_dir[dir] &= 0x78;
	if (res_next > -1) flag->other_end_dir[dir] |= BIT(7) | res_next;
}

/* Cancel transport of resources to building at flag. */
void
flag_cancel_transported_stock(flag_t *flag, int res)
{
	const int res_stock_type[] = {
		-1,
		0, 0, 0, 0, 0, 0, 1, 0,
		-1, 1, 1, 1, 0, 1, 1, -1,
		-1, -1, -1, -1, -1, -1, -1,
		-1, -1
	};

	if (res_stock_type[res] >= 0 &&
	    1/*FLAG_INDEX(flag) != ..*/) {
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
		if (!BUILDING_IS_DONE(building) ||
		    (BUILDING_TYPE(building) != BUILDING_STOCK &&
		     BUILDING_TYPE(building) != BUILDING_CASTLE)) {
			if (res_stock_type[res] == 0) {
				building->stock1 -= 1;
			} else {
				building->stock2 -= 1;
			}
		}
	}
}
