#pragma once

template <typename T> struct Array {
    Arena* arena;
    T* items;
    u64 count;

    static Array<T> make(Arena* arena) {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "Array only supports trivial, trivially copyable types"
        );
        assert_msg(arena != nullptr, "Array arena must not be null!");

        Array<T> arr = {};
        arr.arena = arena;
        return arr;
    }

    static Array<T> make(Arena* arena, u64 count) {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "Array only supports trivial, trivially copyable types"
        );
        assert_msg(arena != nullptr, "Array arena must not be null!");

        Array<T> arr = make(arena);
        arr.items = (count > 0) ? arena->push<T>(count) : nullptr;
        arr.count = count;
        return arr;
    }

    Array<T> copy(const T* source, u64 source_count) const {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "Array only supports trivial, trivially copyable types"
        );
        assert_msg(arena != nullptr, "Array arena must not be null!");

        Array<T> arr = make(arena, source_count);
        if (source_count > 0) {
            assert_msg(
                source != nullptr,
                "Array copy source must not be null!"
            );
            memcpy(arr.items, source, arr.size_bytes());
        }
        return arr;
    }

    T& operator[](u64 i) {
        assert_msg(i < count, "Array index out of range!");
        return items[i];
    }

    const T& operator[](u64 i) const {
        assert_msg(i < count, "Array index out of range!");
        return items[i];
    }

    T* begin() {
        return items;
    }

    T* end() {
        return (items != nullptr) ? (items + count) : nullptr;
    }

    const T* begin() const {
        return items;
    }

    const T* end() const {
        return (items != nullptr) ? (items + count) : nullptr;
    }

    u64 size_bytes() const {
        return count * sizeof(T);
    }
};

template <typename T> struct ArrayListNode {
    ArrayListNode<T>* next;
    T value;
};

template <typename T> struct ArrayList {
    Arena* arena;
    u64 count;
    ArrayListNode<T>* first;
    ArrayListNode<T>* last;

    static ArrayList<T> make(Arena* arena) {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "ArrayList only supports trivial, trivially copyable types"
        );
        assert_msg(arena != nullptr, "ArrayList arena must not be null!");
        ArrayList<T> list = {};
        list.arena = arena;
        return list;
    }

    ArrayListNode<T>* push(const T& value) {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "ArrayList only supports trivial, trivially copyable types"
        );
        assert_msg(arena != nullptr, "ArrayList arena must not be null!");

        ArrayListNode<T>* node = arena->push<ArrayListNode<T>>();
        node->value = value;
        node->next = nullptr;

        if (last != nullptr) {
            last->next = node;
        } else {
            first = node;
        }
        last = node;
        u64 new_count = 0;
        bool count_overflow = u64_add_overflow(count, 1ULL, &new_count);
        assert_msg(!count_overflow, "ArrayList count overflow!");
        count = new_count;
        return node;
    }

    Array<T> to_array() const {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "ArrayList only supports trivial, trivially copyable types"
        );
        assert_msg(arena != nullptr, "ArrayList arena must not be null!");

        Array<T> result = Array<T>::make(arena, count);
        u64 index = 0;
        for (ArrayListNode<T>* node = first; node != nullptr;
             node = node->next) {
            result.items[index++] = node->value;
        }
        assert_msg(
            index == count,
            "ArrayList count does not match node chain!"
        );
        return result;
    }
};
