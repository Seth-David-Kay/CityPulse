typedef f32 LinkValue_Priority;

typedef struct {
	// shoot forgot about the (using) keyword, re-implement
	Link_Id link_id;
	LinkValue_Priority priority;
} LinkValue;
typedef Array(LinkValue) Array_LinkValue;

// Binary Heap
typedef struct {
	Arena *arena;
	Array_LinkValue heapArray;
} MinHeap;

static MinHeap minheap_init(Arena* arena){
	MinHeap heap = { .arena = arena };
	return heap;
}
static u64 find_parent(u64 i){return (i - 1) / 2; }
static u64 left_child(u64 i){return 2 * i + 1; }
static u64 right_child(u64 i){return 2 * i + 2; }
static bool minheap_insert_key(MinHeap* minheap, LinkValue value){
	Array_LinkValue *heap = &minheap->heapArray;
	u64 i = heap->count;
	push(minheap->arena, heap, value);
	while (i != 0 && heap->data[i].priority < heap->data[find_parent(i)].priority){
		u64 parent = find_parent(i);
		LinkValue temp = heap->data[i];
		heap->data[i] = heap->data[parent];
		heap->data[parent] = temp;
		i = parent;
	}
	return true;
}

static LinkValue minheap_pop_key(MinHeap* minheap){
	Array_LinkValue *heap = &minheap->heapArray;
	if (heap->count <= 0)
		return (LinkValue){0};
	LinkValue root = heap->data[0];
	swap_remove(heap, 0);
	u64 i = 0;
	while (true){
		u64 left = left_child(i);
		u64 right = right_child(i);
		u64 smallest = i;
		if (left < heap->count && heap->data[left].priority < heap->data[smallest].priority)
			smallest = left;
		if (right < heap->count && heap->data[right].priority < heap->data[smallest].priority)
			smallest = right;
		if (smallest == i)
			break;
		LinkValue temp = heap->data[i];
		heap->data[i] = heap->data[smallest];
		heap->data[smallest] = temp;
		i = smallest;
	}
	return root;
}

static void minheap_test(void){
	Arena arena = arena_init(10 * 1020);
	MinHeap heap = minheap_init(&arena);
	LinkValue linkvalue1 = {.link_id.i = 0, .priority = 0};
	LinkValue linkvalue2 = {.link_id.i = 1, .priority = 1};
	LinkValue linkvalue3 = {.link_id.i = 2, .priority = 2};
	minheap_insert_key(&heap, linkvalue3);
	minheap_insert_key(&heap, linkvalue2);
	minheap_insert_key(&heap, linkvalue1);
	minheap_insert_key(&heap, linkvalue2);
	minheap_insert_key(&heap, linkvalue1);
	minheap_insert_key(&heap, linkvalue3);
	minheap_insert_key(&heap, linkvalue1);
	minheap_insert_key(&heap, linkvalue2);
	minheap_insert_key(&heap, linkvalue2);
	minheap_insert_key(&heap, linkvalue3);
	minheap_insert_key(&heap, linkvalue1);
	minheap_insert_key(&heap, linkvalue2);
	minheap_insert_key(&heap, linkvalue1);
	minheap_insert_key(&heap, linkvalue2);
	minheap_insert_key(&heap, linkvalue3);
	minheap_insert_key(&heap, linkvalue1);

	while(heap.heapArray.count > 0){
		LinkValue link = minheap_pop_key(&heap);
		print("%u ", (u32)link.priority);
	}
	print("\n");
	print("0 0 0 0 0 0 1 1 1 1 1 1 2 2 2 2 : Expected\n");
}
