#ifndef ArrayList_h
#define ArrayList_h

#include "DebugUtil.h"

// https://de.wikibooks.org/wiki/C%2B%2B-Programmierung/_Templates/_Klassentemplates
// https://stackoverflow.com/questions/8752837/undefined-reference-to-template-class-constructor
template <typename T> class ArrayList {
  public:
  
    ArrayList(void) {
      this->size = 0;
    }
    
    ~ArrayList(void) {
      free(list);
    }

    void add(T item) {
      DEBUG_PRINTLN(" ArrayList.add(0x%04X)", item);
      T* newlist = (T*)malloc((size + 1) * sizeof(T));

      if (size > 0) {
        // umkopieren
        for (int i = 0; i < size; i++) {
          newlist[i] = list[i];
        }
      }

      // insert new value
      newlist[size] = item;
      if (list != NULL) {
        free(list); // free memory of old list
      }
      list = newlist; // replace free'd list with newlist
      size++;
//      DEBUG_RAM();
    }

    bool remove(int index) {
      DEBUG_PRINTLN(" ArrayList.remove(%i)", index);
      // if there is nothing to remove, just return
      if (size == 0 || index > size) {
//        DEBUG_RAM();
        return false;
      }

      // check if we are about to clear the list --> reach size=0 after this removal
      if (size == 1) {
        clear();
//        DEBUG_RAM();
        return true;
      }

      // we did not yet reach size=0, so we need to shring the array by creating new array
      T* newlist = (T*) malloc((size - 1) * sizeof(T));

      //From Begining
      for (int i = 0; i < index; i++) {
        newlist[i] = list[i];
      }
      //From next Index
      for (int i = index; i <= size - 1; i++) {
        newlist[i] = list[i + 1];
      }


      free(list); // free memory of old list
      list = newlist; // replace free'd list with newlist

      size = size - 1;
//      DEBUG_RAM();
      return true;
    }

    bool set(int index, T item) {
      if (size == 0 || index > size) {
        return false;
      }
      list[index] = item;
//      DEBUG_PRINTLN(" ArrayList.set(%i)=0x%04X", index, item);
//      DEBUG_RAM();
      return true;
    }

    // const, because of error: error: passing 'const ArrayList<short unsigned int>' as 'this' argument of 'bool ArrayList<T>::get(int, T&) [with T = short unsigned int]' discards qualifiers [-fpermissive]
    // see: https://stackoverflow.com/questions/5973427/error-passing-xxx-as-this-argument-of-xxx-discards-qualifiers
    bool get(int index, T& item) const {
      if (size == 0 || index > size) {
        return false;
      }
      item = this->list[index];
//      DEBUG_RAM();
//      DEBUG_PRINTLN(" ArrayList.get(%i)=0x%04X", index, item);
      return true;
    }

    void clear() {
//      DEBUG_PRINTLN(" ArrayList.clear");
      size = 0;
      free(list); // free memory of old list
      list = NULL;
    }

    void set(T* list) {
      free(this->list);
      this->list = list;
    }
    
    T* get() {
      return list;
    }

    int getSize() const {
//      DEBUG_PRINTLN(" ArrayList.getSize=%i", size);
      return size;
    }
    
    void log() {
      DEBUG_PRINTLN(" ArrayList Dump. size=%i", size);
      for (int i = 0; i < size; i++) {      
        DEBUG_PRINTLN("   [%i] 0x%04X", i, list[i]);
      }
    }

  private:
    T* list; // pointer, as T is an array
    int size;
};

#endif
