#include "product.h"
#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

// This is where the global variables actually live in memory.
ProductNode* head = nullptr;
int nextId = 1;

// ---------------- CREATE ----------------
void addProduct(string name, int categoryId, double price, int stock) {
    ProductNode* node = new ProductNode;
    node->data.id = nextId;
    node->data.name = name;
    node->data.categoryId = categoryId;
    node->data.price = price;
    node->data.stock = stock;
    node->next = nullptr;
    nextId = nextId + 1;

    if (head == nullptr) {
        head = node;
    } else {
        ProductNode* current = head;
        while (current->next != nullptr) {
            current = current->next;
        }
        current->next = node;
    }

    cout << "Product added with id " << node->data.id << endl;
}

// ---------------- UPDATE ----------------
void editProduct(int id, string name, int categoryId, double price, int stock) {
    ProductNode* current = head;
    while (current != nullptr) {
        if (current->data.id == id) {
            current->data.name = name;
            current->data.categoryId = categoryId;
            current->data.price = price;
            current->data.stock = stock;
            cout << "Product " << id << " updated" << endl;
            return;
        }
        current = current->next;
    }
    cout << "Product " << id << " not found" << endl;
}

// ---------------- DELETE ----------------
void deleteProduct(int id) {
    ProductNode* current = head;
    ProductNode* prev = nullptr;

    while (current != nullptr && current->data.id != id) {
        prev = current;
        current = current->next;
    }

    if (current == nullptr) {
        cout << "Product " << id << " not found" << endl;
        return;
    }

    if (prev == nullptr) {
        head = current->next; // deleting the first node
    } else {
        prev->next = current->next;
    }

    delete current;
    cout << "Product " << id << " deleted" << endl;
}

// ---------------- READ ----------------
void viewProducts()
{
    if (head == nullptr)
    {
        cout << "\n[!] No products available in the inventory.\n"
             << endl;
        return;
    }
    cout << "\n===============================================================================\n";
    cout << left
         << setw(8) << "ID"
         << setw(25) << "Name"
         << setw(15) << "Category ID"
         << setw(15) << "Price"
         << setw(10) << "Stock" << "\n";
    cout << "-------------------------------------------------------------------------------\n";
    ProductNode *current = head;
    while (current != nullptr)
    {
        cout << left
             << setw(8) << current->data.id
             << setw(25) << current->data.name
             << setw(15) << current->data.categoryId
             << "$" << setw(14) << fixed << setprecision(2) << current->data.price
             << setw(10) << current->data.stock << "\n";
        current = current->next;
    }
    cout << "\n===============================================================================\n";
}

// Reads one CSV line field-by-field using getline with a comma as the
// stop character, instead of the default newline. Same getline() you
// already use, just told to stop early.
void loadProducts() {
    ifstream in("product.csv");
    if (!in.is_open()) {
        return; // no file yet, nothing to load
    }

    string headerLine;
    getline(in, headerLine); // skip the header row ("id,name,categoryId,price,stock")

    string idText;
    // getline normally stops at a newline. Give it a THIRD argument (a
    // character) and it stops at THAT character instead. So this line
    // reads everything up to the next comma, instead of the whole line.
    while (getline(in, idText, ',')) {
        if (idText.empty()) continue;

        string name, catText, priceText, stockText;
        getline(in, name, ',');       // reads up to the 2nd comma
        getline(in, catText, ',');    // reads up to the 3rd comma
        getline(in, priceText, ',');  // reads up to the 4th comma
        getline(in, stockText);       // no comma given, so this reads to the END of the line

        ProductNode* node = new ProductNode;
        node->data.id = stoi(idText);
        node->data.name = name;
        node->data.categoryId = stoi(catText);
        node->data.price = stod(priceText);
        node->data.stock = stoi(stockText);
        node->next = nullptr;

        if (head == nullptr) {
            head = node;
        } else {
            ProductNode* current = head;
            while (current->next != nullptr) {
                current = current->next;
            }
            current->next = node;
        }

        if (node->data.id >= nextId) {
            nextId = node->data.id + 1;
        }
    }

    in.close();
}

void saveProducts()
{
    ofstream out("product.csv");
    if (!out.is_open())
    {
        cout << "Could not open file to save." << endl;
        return;
    }

    out << "id,name,categoryId,price,stock" << endl; // header row

    ProductNode *current = head;
    while (current != nullptr)
    {
        out << current->data.id << ","
            << current->data.name << ","
            << current->data.categoryId << ","
            << current->data.price << ","
            << current->data.stock << endl;
        current = current->next;
    }

    out.close();
    cout << "Products saved to product.csv" << endl;
}

// ---------------- SORTING (Bubble Sort) ----------------
void sortByPrice() {
    if (head == nullptr) return;

    bool swapped = true;
    while (swapped == true) {
        swapped = false;
        ProductNode* current = head;
        while (current->next != nullptr) {
            if (current->data.price > current->next->data.price) {
                // swap the data of the two nodes
                Product temp = current->data;
                current->data = current->next->data;
                current->next->data = temp;
                swapped = true;
            }
            current = current->next;
        }
    }
    cout << "Products sorted by price." << endl;
}

// ---------------- CLEANUP ----------------
void freeAllProducts() {
    ProductNode* current = head;
    while (current != nullptr) {
        ProductNode* toDelete = current;
        current = current->next;
        delete toDelete;
    }
    head = nullptr;
}