#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <X11/Xlib.h>


typedef struct
{
  Window frame; // The title bar, border, controls, etc
  Window child; // The window defined by the client application
  size_t index;
} Client; // The actual whole window displayed

typedef struct {
    Client* clients;  
    size_t size;      
    size_t capacity;  
} DynamicArray;

DynamicArray* createDynamicArray(size_t);
void addClient(DynamicArray*, Client);
int findClient(DynamicArray*, Window, Client*);
void removeClient(DynamicArray*, size_t);
void freeDynamicArray(DynamicArray*);
