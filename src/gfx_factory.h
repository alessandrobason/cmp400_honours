#pragma once

extern struct VirtualAllocator g_gfx_arena;

template<typename T>
struct GFXFactory {
	GFXFactory() = default;
	~GFXFactory() {
		// check that we remembered to call cleanup
		assert(!head);
	}

	struct Node : public T {
		Node *next = nullptr;
	};

	T *getNew() {
		Node *node = (Node *)g_gfx_arena.alloc(sizeof(Node));
		node->next = head;
		head = node;
		return node;
	}

	void popLast() {
		g_gfx_arena.rewind(sizeof(Node));
		head = head->next;
	}

	void remove(T *ptr) {
		if (!ptr) return;
		ptr->cleanup();

		Node *node = head;
		Node *prev = nullptr;
		while (node) {
			if (node == ptr) {
				if (prev) prev->next = node->next;
				else      head = node->next;
				break;
			}

			prev = node;
			node = node->next;
		}
	}

	void cleanup() {
		Node *node = head;
		while (node) {
			node->cleanup();
			node = node->next;
		}
		head = nullptr;
	}

	Node *head = nullptr;
};
