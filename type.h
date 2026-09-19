#ifndef TYPES_H
#define TYPES_H
#include <string>
using namespace std;

struct Category {
    int id;
    string name;
    string description;
};

struct Product {
    int id;
    string name;
    int categoryId;
    double price;
    int stock;
};

struct Customer {
    int id;
    string name;
    string phone;
};

struct Order {
    int id;
    int customerId;
    int productId;
    int quantity;
    string date;
};

#endif