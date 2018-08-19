#ifndef ArrayList_h
#define ArrayList_h

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
    }

    bool remove(int index) {
      // if there is nothing to remove, just return
      if (size == 0 || index > size) {
        return false;
      }

      // check if we are about to clear the list --> reach size=0 after this removal
      if (size == 1) {
        clear();
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
      return true;
    }

    bool set(int index, T item) {
      if (size == 0 || index > size) {
        return false;
      }
      list[index] = item;
      return true;
    }

    // const, because of error: error: passing 'const ArrayList<short unsigned int>' as 'this' argument of 'bool ArrayList<T>::get(int, T&) [with T = short unsigned int]' discards qualifiers [-fpermissive]
    // see: https://stackoverflow.com/questions/5973427/error-passing-xxx-as-this-argument-of-xxx-discards-qualifiers
    bool get(int index, T& item) const {
      if (size == 0 || index > size) {
        return false;
      }
      item = this->list[index];
      return true;
    }

    void clear() {
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
      return size;
    }
    
    void log() {
      char charVal[20];
      for (int i = 0; i < size; i++) {
        sprintf(charVal, fmt, i, list[i]);
        SerialUSB.println(charVal);
      }
    }

  private:
    const char* fmt = "[%i] 0x%04X";
    T* list; // pointer, as T is an array
    int size;
};

#endif
