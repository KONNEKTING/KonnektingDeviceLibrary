/*
 *    This file is part of KONNEKTING Knx Device Library.
 * 
 *
 *    The KONNEKTING Knx Device Library is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// File : ArrayList.h
// Initial Author : Alexander Christian <info(at)root1.de>
// Description : Generic array list for simple numeric types like word, int, ...
// Module dependencies : none
#ifndef ArrayList_h
#define ArrayList_h

#include "Arduino.h"

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

    void addItem(T* item) {
      T **newlist = (T**)malloc((size + 1) * sizeof(T*));

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
      size = size + 1;
    }

    bool removeItem(int index) {
      // if there is nothing to remove, just return
      if (size == 0 || index > size) {
        return false;
      }

      // check if we are about to clear the list --> reach size=0 after this removal
      if (size == 1) {
        clearList();
        return true;
      }

      // we did not yet reach size=0, so we need to shring the array by creatingnew array
      T **newlist = (T**)malloc((size - 1) * sizeof(T*));

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

    bool setItem(int index, T* item) {
      if (size == 0 || index > size) {
        return false;
      }
      list[index] = item;
      return true;
    }

    bool getItem(int index, T* item) {
      if (size == 0 || index > size) {
        return false;
      }
      item = this->list[index];
      return true;
    }

    void clearList() {
      size = 0;
      free(list); // free memory of old list
      list = NULL;
    }

    void setList(T** list) {
      free(this->list);
      this->list = list;
    }
    
    T** getList() {
      return list;
    }

    int getSize() {
      return size;
    }
    
    void logList() {
      char charVal[20];
      for (int i = 0; i < size; i++) {
        sprintf(charVal, fmt, i, list[i]);
        Serial.println(charVal);
      }
    }

  private:
    const char* fmt = "[%i] 0x%08X";
    T** list;
    int size;
};

#endif
