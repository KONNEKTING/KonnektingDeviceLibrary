#ifndef HashMap_h
#define HashMap_h

#include "DebugUtil.h"

// http://www.codeadventurer.de/?p=2091
// https://de.wikibooks.org/wiki/C%2B%2B-Programmierung/_Templates/_Klassentemplates
// https://stackoverflow.com/questions/8752837/undefined-reference-to-template-class-constructor

/*
 * Hashmap Implementation. 
 * Limitations: 
 *  - "Value" can be chosen freely
 *  - "Key" is fixed to uint16_t.
 *  - size is fixed and cannot grow or shrink
 */
template <typename V>
class HashMap {
   public:
    HashMap(void) {
        this->size = 0;
    }

    ~HashMap(void) {
        free(list);
    }

    void init(uint8_t size) {
        V* list = (V*)malloc((size) * sizeof(V));
        this->size = size;
    }

    void put(uint16_t key, V value) {
        uint16_t index = key % size;

        DEBUG_PRINTLN(" HashMap.put(key=0x%04X value=0x%04x index=%d)", key, value, index);
        // insert new value
        list[index] = value;
    }

    // const, because of error: error: passing 'const ArrayList<short unsigned int>' as 'this' argument of 'bool ArrayList<T>::get(int, T&) [with T = short unsigned int]' discards qualifiers [-fpermissive]
    // see: https://stackoverflow.com/questions/5973427/error-passing-xxx-as-this-argument-of-xxx-discards-qualifiers
    bool get(uint16_t key, V& value) const {
        uint16_t index = key % size;

        value = this->list[index];
        DEBUG_PRINTLN(" HashMap.get(key=0x%04X value=0x%04x index=%d)", key, value, index);
        return true;
    }

    V* get() {
        return list;
    }

    int getSize() const {
        return size;
    }

    void log() {
        DEBUG_PRINTLN(" HashMap Dump. size=%i", size);
        for (int i = 0; i < size; i++) {
            DEBUG_PRINTLN("   [%i] 0x%04X", i, list[i]);
        }
    }

   private:
    V* list;  // pointer, as V is an array
    int size;
};

#endif
