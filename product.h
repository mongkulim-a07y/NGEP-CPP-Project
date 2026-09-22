#ifndef PRODUCT_H
#define PRODUCT_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>
#include "type.h"
using namespace std;

// This class does NOT read from cin. All input-gathering (prompts) lives in
// main.cpp (see promptAddProduct / promptEditProduct / promptDeleteProduct),
// same convention as CategoryList / CustomerList / LoginList.
//
// This replaces the old global `ProductNode* head` + `int nextId` +
// free-function design (product.h/product.cpp). Those globals could be
// poked directly from anywhere that included the header, with nothing
// stopping two different parts of the program from corrupting the list.
// Wrapping the same data in a class keeps everything encapsulated, exactly
// like the rest of the project.
class ProductList
{
    // =====================================================================
    // Data structure
    // =====================================================================
private:
    struct ProductNode
    {
        Product data;
        ProductNode *next;
        ProductNode(Product value) : data(value), next(nullptr) {}
    };

    ProductNode *head;
    ProductNode *tail;
    int nextId; // next id to hand out; kept in sync as records are loaded/added

    static constexpr const char *CSV_PATH = "CsvFile/product.csv";

    // =====================================================================
    // Private helpers
    // =====================================================================

    // Shared lookup used by findProduct()/editProduct()/deleteProduct().
    ProductNode *findNode(int id) const
    {
        ProductNode *temp = head;
        while (temp != nullptr)
        {
            if (temp->data.id == id)
                return temp;
            temp = temp->next;
        }
        return nullptr;
    }

    // Appends a node (O(1) via tail) and keeps nextId ahead of every id seen.
    void appendNode(const Product &p)
    {
        ProductNode *newNode = new ProductNode(p);
        if (head == nullptr)
        {
            head = tail = newNode;
        }
        else
        {
            tail->next = newNode;
            tail = newNode;
        }
        if (p.id >= nextId)
            nextId = p.id + 1;
    }

public:
    // =====================================================================
    // Construction / destruction
    // =====================================================================

    ProductList() : head(nullptr), tail(nullptr), nextId(1)
    {
        loadProducts();
    }

    ~ProductList()
    {
        freeAllProducts();
    }

    // Prevent accidental copies (raw pointers would double-free otherwise)
    ProductList(const ProductList &) = delete;
    ProductList &operator=(const ProductList &) = delete;

    vector<Product> getAllProducts() const
    {
        vector<Product> result;
        ProductNode *temp = head;
        while (temp != nullptr)
        {
            result.push_back(temp->data);
            temp = temp->next;
        }
        return result;
    }
    void freeAllProducts()
    {
        ProductNode *current = head;
        while (current != nullptr)
        {
            ProductNode *toDelete = current;
            current = current->next;
            delete toDelete;
        }
        head = nullptr;
        tail = nullptr;
    }

    // =====================================================================
    // File I/O
    // =====================================================================

    void loadProducts()
    {
        ifstream in(CSV_PATH);
        if (!in.is_open())
            return; // no file yet, nothing to load

        freeAllProducts();
        nextId = 1;

        string headerLine;
        getline(in, headerLine); // skip the header row ("id,name,categoryId,price,stock")

        string idText;
        while (getline(in, idText, ','))
        {
            if (idText.empty())
                continue;

            string name, catText, priceText, stockText;
            getline(in, name, ',');
            getline(in, catText, ',');
            getline(in, priceText, ',');
            getline(in, stockText); // reads to end of line (no comma given)

            try
            {
                Product p;
                p.id = stoi(idText);
                p.name = name;
                p.categoryId = stoi(catText);
                p.price = stod(priceText);
                p.stock = stoi(stockText);
                appendNode(p);
            }
            catch (...)
            {
                continue; // skip malformed rows safely
            }
        }

        in.close();
    }

    void saveProducts()
    {
        ofstream out(CSV_PATH);
        if (!out.is_open())
        {
            cout << "Could not open file to save." << endl;
            return;
        }

        out << "id,name,categoryId,price,stock" << endl;

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

    // =====================================================================
    // ID management / lookup
    // =====================================================================

    // Predicts the ID the NEXT addProduct() call should use. Robust across
    // deletions since it's tracked from the max id ever seen, not the count.
    int getNextProductId() const
    {
        return nextId;
    }

    int getProductCount() const
    {
        int count = 0;
        ProductNode *temp = head;
        while (temp != nullptr)
        {
            count++;
            temp = temp->next;
        }
        return count;
    }

    // Returns a pointer to the live product, or nullptr if not found. Lets
    // callers (e.g. an "edit" prompt) show current details before asking
    // for new ones.
    Product *findProduct(int id) const
    {
        ProductNode *node = findNode(id);
        return node ? &node->data : nullptr;
    }

    // =====================================================================
    // CRUD operations (pure data - no cin, no prompts)
    // =====================================================================

    // Adds a new product with the given data and returns its assigned id.
    // The caller typically gets that id from getNextProductId() beforehand
    // if it needs to know it ahead of time; either way this returns it too.
    int addProduct(const string &name, int categoryId, double price, int stock)
    {
        Product p;
        p.id = nextId;
        p.name = name;
        p.categoryId = categoryId;
        p.price = price;
        p.stock = stock;
        appendNode(p);
        return p.id;
    }

    // Reduces stock by `quantity` if the product exists and has enough.
    // Returns true on success, false if the product isn't found or there
    // isn't enough stock (in which case nothing is changed). Does NOT save
    // to disk itself - matches editProduct()'s convention; call
    // saveProducts() afterwards.
    bool reduceStock(int id, int quantity)
    {
        ProductNode *node = findNode(id);
        if (node == nullptr)
            return false;
        if (node->data.stock < quantity)
            return false;

        node->data.stock -= quantity;
        return true;
    }

    // Returns true if the product was found and updated, false otherwise.
    bool editProduct(int id, const string &name, int categoryId, double price, int stock)
    {
        ProductNode *node = findNode(id);
        if (node == nullptr)
            return false;

        node->data.name = name;
        node->data.categoryId = categoryId;
        node->data.price = price;
        node->data.stock = stock;
        return true;
    }

    // Returns true if the product was found and removed, false otherwise.
    bool deleteProduct(int id)
    {
        ProductNode *current = head;
        ProductNode *prev = nullptr;

        while (current != nullptr && current->data.id != id)
        {
            prev = current;
            current = current->next;
        }

        if (current == nullptr)
            return false;

        if (prev == nullptr)
            head = current->next;
        else
            prev->next = current->next;

        if (current == tail)
            tail = prev;

        delete current;
        return true;
    }

    // =====================================================================
    // Display (pure output, no input - stays here like viewCategories())
    // =====================================================================

    void viewProducts() const
    {
        if (head == nullptr)
        {
            cout << "\n[!] No products available in the inventory.\n"
                 << endl;
            return;
        }
        printTable(head);
    }

    // Displays products sorted by price, LOW to HIGH. This is
    // display-only: it sorts a temporary copy and never touches the
    // real list or the CSV, so the underlying order/order-of-IDs is
    // unaffected.
    void sortByPrice() const
    {
        if (head == nullptr)
        {
            cout << "No products to display.\n";
            return;
        }

        // Deep-copy the list so the original is never touched.
        ProductNode *tempHead = new ProductNode(head->data);
        ProductNode *tempTail = tempHead;
        ProductNode *original = head->next;

        while (original != nullptr)
        {
            tempTail->next = new ProductNode(original->data);
            tempTail = tempTail->next;
            original = original->next;
        }

        // Bubble sort the copy by price.
        bool swapped = true;
        while (swapped)
        {
            swapped = false;
            ProductNode *current = tempHead;
            while (current != nullptr && current->next != nullptr)
            {
                if (current->data.price > current->next->data.price)
                {
                    Product temp = current->data;
                    current->data = current->next->data;
                    current->next->data = temp;
                    swapped = true;
                }
                current = current->next;
            }
        }

        printTable(tempHead);

        while (tempHead != nullptr)
        {
            ProductNode *toDelete = tempHead;
            tempHead = tempHead->next;
            delete toDelete;
        }

        cout << "Products sorted by price." << endl;
    }

private:
    // Shared table-printing routine used by both viewProducts() and
    // sortByPrice(), so the formatting lives in exactly one place.
    static void printTable(ProductNode *listHead)
    {
        cout << "\n===============================================================================\n";
        cout << left
             << setw(8) << "ID"
             << setw(25) << "Name"
             << setw(15) << "Category ID"
             << setw(15) << "Price"
             << setw(10) << "Stock" << "\n";
        cout << "-------------------------------------------------------------------------------\n";

        ProductNode *current = listHead;
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

    
};

#endif