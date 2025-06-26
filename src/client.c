#include "client.h"

DynamicArray* createDynamicArray(size_t initialCapacity) {
    DynamicArray* array = (DynamicArray*)malloc(sizeof(DynamicArray));
    array->clients = (Client*)malloc(initialCapacity * sizeof(Client));
    array->size = 0;
    array->capacity = initialCapacity;
    return array;
}

void addClient(DynamicArray* array, Client client) {
    if (array->size >= array->capacity) {
        
        array->capacity *= 2;
        array->clients = (Client*)realloc(array->clients, array->capacity * sizeof(Client));
    }
    array->clients[array->size++] = client;
}

void removeClient(DynamicArray* array, size_t index) {
    if (index < array->size) {
        
        for (size_t i = index; i < array->size - 1; i++) {
            array->clients[i] = array->clients[i + 1];
        }
        array->size--;  
    } else {
        printf("Index out of bounds\n");
    }
}

void freeDynamicArray(DynamicArray* array) {
    free(array->clients);
    free(array);
}

int findClient(DynamicArray* array, Window compWin, Client* outClient)
{
  if (array != NULL)
  {
    for (size_t i = 0; i < array->size; i++)
    {
      Client client = array->clients[i];
      if (client.child == compWin || client.frame == compWin)
      {
        client.index = i;
        *outClient = client;
        return 0;
      }
    }
  }
  return 1;
}
