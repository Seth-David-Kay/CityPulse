#define PLATFORM_NONE 1
#include "base/base.c"

static void program(void){
	Arena arena = arena_init(10 * 1020);
	Map_i32 map = {0};
	map_make(&arena, &map, 100);
	i32 num = 12123;
	String key = string("key");
	map_put(&map, key, &num);
	i32 *result = map_get(&map, key).pointer;
	print("%\n", fmt(i32, *result));
}
