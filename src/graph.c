typedef struct { u32 i; } Link_Id;

typedef struct {
	Link_Id *data;
	u32 count;
} Link_Ids;
typedef Array(Link_Id) Array_Link_Id;

/*
A generic graph structure for maximum dynamism. It is made up of linkds, acting as
nodes and edges where they need to, to encapsulate all unusual traffic patterns possible.
Each link has meta data, such as entry and exit conditions, and can act as lanes, roads,
intersection paths, anything that can be a traffic complication. Also holds complex graph
definitions of cars and links to make a completed graphs based on real-world geometries.
*/

typedef u32 Node_Flags;
enum {
	Node_Flag_DEAD = 1 << 0,
};

typedef struct {
	V2 position;
	Node_Flags flags;
} Node;
typedef Array(Node) Array_Node;

typedef struct {
	Node *nodes;
	u64 count;

	Node *node;
	u64 i;
} Node_Iterator;

static Node_Iterator node_iterator(Node *nodes, u64 count) {
	Node_Iterator it = { .nodes = nodes, .count = count };
	return it;
}

static bool node_iterate(Node_Iterator *it) {
	do {
		it->i += 1;
		it->node = &it->nodes[it->i];
	} while (it->node->flags & Node_Flag_DEAD);
	bool keep_going = it->i < it->count;
	return keep_going;
}

typedef struct { u32 i; } Node_Id;

typedef struct {
	Node_Id *data;
	u32 count;
} Node_Ids;
typedef Array(Node_Id) Array_Node_Id;

typedef u8 Link_Flags;
enum {
    Link_Flag_STOP_SIGN            = 1 << 0,
    Link_Flag_TRAFFIC_LIGHT_RED    = 1 << 1,
    Link_Flag_TRAFFIC_LIGHT_YELLOW = 1 << 2,
    Link_Flag_TRAFFIC_LIGHT_GREEN  = 1 << 3,
    Link_Flag_SPEEDBUMP            = 1 << 4,
    Link_Flag_BLOCKED              = 1 << 5,
    Link_Flag_DEAD				   = 1 << 6,
};
#define LINK_FLAG_ANY_STOP (Link_Flag_STOP_SIGN | Link_Flag_TRAFFIC_LIGHT_RED | Link_Flag_BLOCKED | Link_Flag_DEAD)
#define LINK_FLAG_ANY_TRAFFIC_LIGHT (Link_Flag_TRAFFIC_LIGHT_RED | Link_Flag_TRAFFIC_LIGHT_YELLOW | Link_Flag_TRAFFIC_LIGHT_GREEN)

typedef struct {
	Node_Id from;
	Node_Id to;
	Link_Flags flags;
	u8 intersection_id; //0 = none, 1+ = belongs to that intersection
	// NOTE(felix): consider capacity field
	// NOTE(felix): consider traversal_cost field (higher cost models curvy links)
} Link;
typedef Array(Link) Array_Link;

typedef struct {
	Link *links;
	u64 count;

	Link *link;
	u64 i;
} Link_Iterator;

static Link_Iterator link_iterator(Link *links, u64 count) {
	Link_Iterator it = { .links = links, .count = count };
	return it;
}

static bool link_iterate(Link_Iterator *it) {
	do {
		it->i += 1;
		it->link = &it->links[it->i];
	} while (it->link->flags & Link_Flag_DEAD);
	bool keep_going = it->i < it->count;
	return keep_going;
}

typedef struct {
    Link *links;
    Node *nodes;
    u32 link_count;
    u32 node_count;

    Node_Ids entrances;
    Node_Ids exits;
} Map;

typedef struct {
	Array(Link) links;
	Array(Node) nodes;

	Array_Link_Id temp_link_ids;
	Node_Ids entrances;
	Node_Ids exits;
} Graph;

static Graph graph_init(Map map) {
	Graph graph = {0};

	graph.nodes.data = map.nodes;
	graph.nodes.count = map.node_count;
	graph.nodes.capacity = map.node_count;

	graph.links.data = map.links;
	graph.links.count = map.link_count;
	graph.links.capacity = map.link_count;

	graph.entrances = map.entrances;
	graph.exits = map.exits;

	return graph;
}

static f32 link_length(Graph graph, Link link) {
	Node from = graph.nodes.data[link.from.i];
	Node to = graph.nodes.data[link.to.i];
	f32 length = v2_len(v2_sub(to.position, from.position));
	return length;
}

static f32 cost_to_reach_through(Graph graph, Node_Id through) {
	f32 cost = 0;

	for (Link_Iterator it = link_iterator(graph.links.data, graph.links.count); link_iterate(&it);) {
		Link link = *it.link;

		if (link.to.i == through.i) {
			cost += 50.f * (f32)!!(link.flags & Link_Flag_STOP_SIGN);
			cost += 100.f * (f32)!!(link.flags & Link_Flag_TRAFFIC_LIGHT_RED);
			cost += 30.f * (f32)!!(link.flags & Link_Flag_TRAFFIC_LIGHT_YELLOW);
			cost += link_length(graph, link) * 0.01f;

			break;
		}
	}

	return cost;
}

static Link_Id link_between_nodes(Graph graph, Node_Id from, Node_Id to) {
	Link_Id link_id = {0};

	for (Link_Iterator it = link_iterator(graph.links.data, graph.links.count); link_iterate(&it);) {
		Link link = *it.link;
		if (link.from.i == from.i && link.to.i == to.i) {
			link_id.i = (u32)it.i;
			break;
		}
	}

	return link_id;
}

static f32 edge_weight(Graph graph, Node_Id from, Node_Id to) {
	Link_Id link_id = link_between_nodes(graph, from, to);
	assert(link_id.i != (u32)-1);

	f32 edge_weight = link_length(graph, graph.links.data[link_id.i]);
	return edge_weight;
}

// TODO(felix): performance
static Node_Ids graph_pathfind(Arena *arena, Graph graph, Node_Id start, Node_Id end) {
	Node_Ids path = { .data = arena_make(arena, graph.nodes.count, Node_Id) };
	Arena scratch = *arena;

	Node_Id *open_set = arena_make(&scratch, graph.nodes.count, Node_Id);
	*open_set = start;
	u32 open_set_count = 1;

	Node_Id *came_from = arena_make(&scratch, graph.nodes.count, Node_Id);

	for (u32 i = 0; i < graph.nodes.count; i += 1) {
		came_from[i] = (Node_Id){(u32)-1};
	}

	// For node n, gScore[n] is the currently known cost of the cheapest path from start to n
	f32 *gscore = arena_make(&scratch, graph.nodes.count, f32);
	for (u32 i = 0; i < graph.nodes.count; i += 1) gscore[i] = FLT_MAX;
	gscore[start.i] = 0;

    // For node n, fScore[n] := gScore[n] + h(n). fScore[n] represents our current best guess as to
    // how cheap a path could be from start to finish if it goes through n.
	f32 *fscore = arena_make(&scratch, graph.nodes.count, f32);
	for (u32 i = 0; i < graph.nodes.count; i += 1) fscore[i] = FLT_MAX;
	fscore[start.i] = cost_to_reach_through(graph, start);

	Node_Id *neighbours = arena_make(&scratch, graph.nodes.count, Node_Id);

	while (open_set_count > 0) {
		u32 current_open_set_index = 0;
		Node_Id current = open_set[0];
		for (u32 i = 0; i < open_set_count; i += 1) {
			if (fscore[open_set[i].i] < fscore[current.i]) {
				current_open_set_index = i;
				current = open_set[i];
			}
		}

		if (current.i == end.i) {
			Node_Id *backwards = arena_make(&scratch, graph.nodes.count, Node_Id);

			backwards[path.count++] = current;
			while (came_from[current.i].i != (u32)-1) {
				current = came_from[current.i];
				backwards[path.count++] = current;
			}

			for (u32 i = 0; i < path.count; i += 1) path.data[i] = backwards[path.count - 1 - i];
			break;
		}

		open_set[current_open_set_index] = open_set[--open_set_count];

		u32 neighbour_count = 0;
		for (Link_Iterator it = link_iterator(graph.links.data, graph.links.count); link_iterate(&it);) {
			Link link = *it.link;
			if (link.from.i == current.i) neighbours[neighbour_count++] = link.to;
		}

		for (u32 i = 0; i < neighbour_count; i += 1) {
			Node_Id neighbour = neighbours[i];

			f32 tentative_gscore = gscore[current.i] + edge_weight(graph, current, neighbour);
			if (tentative_gscore < gscore[neighbour.i]) {
				came_from[neighbour.i] = current;
				gscore[neighbour.i] = tentative_gscore;
				fscore[neighbour.i] = tentative_gscore + cost_to_reach_through(graph, neighbour);

				bool neighbour_in_open_set = false;
				for (u32 j = 0; j < open_set_count; j += 1) {
					if (open_set[j].i == neighbour.i) {
						neighbour_in_open_set = true;
						break;
					}
				}

				if (!neighbour_in_open_set) open_set[open_set_count++] = neighbour;
			}
		}
	}

	return path;
}
