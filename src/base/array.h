#pragma once

#include <climits>
#include <cstring>

#include "base/core.h"
#include "base/arena.h"

template <typename T> struct Array;
template <typename T> inline Array<T> ArrayCreate(Arena* arena, u32 capacity);

template <typename T> struct Array {
    Arena* arena;
    T* items;
    u32 count;
    u32 capacity;

    Array<T> copy(const T* source, u32 source_count) const {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "Array only supports trivial, trivially copyable types"
        );
        assert(arena != nullptr, "Array arena must not be null!");
        assert(
            source_count <= capacity,
            "Array copy source count exceeds array capacity!"
        );

        Array<T> arr = ArrayCreate<T>(arena, capacity);
        if (source_count > 0) {
            assert(
                source != nullptr,
                "Array copy source must not be null!"
            );
            memcpy(arr.items, source, (u64)source_count * sizeof(T));
            arr.count = source_count;
        }
        return arr;
    }

    T& operator[](u64 i) {
        assert(i < count, "Array index out of range!");
        return items[i];
    }

    const T& operator[](u64 i) const {
        assert(i < count, "Array index out of range!");
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

    u64 sizeBytes() const {
        return (u64)count * sizeof(T);
    }
};

template <typename T> inline Array<T> ArrayCreate(Arena* arena, u32 capacity) {
    static_assert(
        IS_TRIVIAL_TYPE(T),
        "Array only supports trivial, trivially copyable types"
    );
    assert(arena != nullptr, "Array arena must not be null!");

    Array<T> arr = {};
    arr.arena = arena;
    arr.capacity = capacity;
    arr.items = (capacity > 0) ? arena->push<T>(capacity) : nullptr;
    arr.count = 0;
    return arr;
}

template <typename T> struct ArrayListNode {
    ArrayListNode<T>* next;
    T value;
};

template <typename T> struct ArrayList {
    Arena* arena;
    u64 count;
    ArrayListNode<T>* first;
    ArrayListNode<T>* last;

    ArrayListNode<T>* push(const T& value) {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "ArrayList only supports trivial, trivially copyable types"
        );
        assert(arena != nullptr, "ArrayList arena must not be null!");

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
        bool count_overflow = AddU64Overflow(count, 1ULL, &new_count);
        assert(!count_overflow, "ArrayList count overflow!");
        count = new_count;
        return node;
    }

    Array<T> toArray() const {
        static_assert(
            IS_TRIVIAL_TYPE(T),
            "ArrayList only supports trivial, trivially copyable types"
        );
        assert(arena != nullptr, "ArrayList arena must not be null!");
        assert(
            count <= (u64)UINT32_MAX,
            "ArrayList count exceeds u32 max for Array capacity!"
        );

        Array<T> result = ArrayCreate<T>(arena, (u32)count);
        u32 index = 0;
        for (ArrayListNode<T>* node = first; node != nullptr;
             node = node->next) {
            result.items[index] = node->value;
            index += 1;
        }
        result.count = index;
        assert(
            (u64)result.count == count,
            "ArrayList count does not match node chain!"
        );
        return result;
    }
};

template <typename T> inline ArrayList<T> ArrayListCreate(Arena* arena) {
    static_assert(
        IS_TRIVIAL_TYPE(T),
        "ArrayList only supports trivial, trivially copyable types"
    );
    assert(arena != nullptr, "ArrayList arena must not be null!");
    ArrayList<T> list = {};
    list.arena = arena;
    return list;
}
