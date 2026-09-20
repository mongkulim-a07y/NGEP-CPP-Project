#ifndef categoryt_hpp
#define categoryt_hpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include "type.h"
using namespace std;

class CategoryList
{
private:
    struct CategoryNode
    {
        Category data;
        CategoryNode *next;

        CategoryNode(Category value) : data(value), next(nullptr) {}
    };

    CategoryNode *head;
    CategoryNode *tail;

public:
    CategoryList() : head(nullptr), tail(nullptr)
    {
        loadCategories();
    }
    ~CategoryList() {
        freeCategory();
    }

    void freeCategory()
    {
        CategoryNode *current = head;
        while (current != nullptr)
        {
            CategoryNode *temp = current;
            current = current->next;
            delete temp;
        }
        head = nullptr;
        tail = nullptr;
    }

    void loadCategories()
    {
        ifstream file;
        file.open("CsvFile/categories.csv");
        if (!file.is_open())
            return;

        string line;
        while (getline(file, line))
        {
            if (line.empty())
                continue;

            stringstream ss(line);
            string idStr, name, desc;

            if (getline(ss, idStr, ',') &&
                getline(ss, name, ',') &&
                getline(ss, desc))
            {

                if (idStr == "ID" || idStr == "id")
                    continue; // Skip CSV header

                try
                {
                    Category c;
                    c.id = stoi(idStr);
                    c.name = name;
                    c.description = desc;

                    // Append node to list using tail
                    CategoryNode *newNode = new CategoryNode(c);
                    if (head == nullptr)
                    {
                        head = tail = newNode;
                    }
                    else
                    {
                        tail->next = newNode;
                        tail = newNode;
                    }
                }
                catch (...)
                {
                    continue; // Skip malformed rows safely
                }
            }
        }
        file.close();
    }

    // 2. SAVE: Write all node data back to CSV file
    void saveCategories()
    {
        ofstream file;
        file.open("CsvFile/categories.csv");
        if (!file.is_open())
        {
            cout << "Error: Could not open file for writing.\n";
            return;
        }

        // CSV Header
        file << "ID,Name,Description\n";

        CategoryNode *temp = head;
        while (temp != nullptr)
        {
            file << temp->data.id << ","
                 << temp->data.name << ","
                 << temp->data.description << "\n";
            temp = temp->next;
        }

        file.close();
        cout << "Categories saved successfully.\n";
    }

    // 3. ADD: Append new node to end using tail (O(1) complexity)
    void addCategory(int id, const string &name, const string &desc)
    {
        Category c{id, name, desc};
        CategoryNode *newNode = new CategoryNode(c);

        if (head == nullptr)
        {
            head = tail = newNode;
        }
        else
        {
            tail->next = newNode;
            tail = newNode;
        }
        cout << "Category added successfully!\n";
    }

    // 4. VIEW: Display all categories
    void viewCategories() const
    {
        cout << "\n+-----+----------------------+------------------------------------------+\n";
        cout << "| " << left << setw(3) << "ID"
             << " | " << left << setw(20) << "Category Name"
             << " | " << left << setw(40) << "Description" << " |\n";
        cout << "+-----+----------------------+------------------------------------------+\n";

        if (head == nullptr)
        {
            cout << "| " << left << setw(68) << "No categories found." << " |\n";
        }
        else
        {
            CategoryNode *current = head;
            while (current != nullptr)
            {
                cout << "| " << left << setw(3) << current->data.id
                     << " | " << left << setw(20) << current->data.name
                     << " | " << left << setw(40) << current->data.description << " |\n";
                current = current->next;
            }
        }

        cout << "+-----+----------------------+------------------------------------------+\n";
    }

    // 5. EDIT: Find by ID and update values
    void editCategory(int id, const string &newName, const string &newDesc)
    {
        CategoryNode *temp = head;
        while (temp != nullptr)
        {
            if (temp->data.id == id)
            {
                temp->data.name = newName;
                temp->data.description = newDesc;
                cout << "Category ID " << id << " updated successfully!\n";
                return;
            }
            temp = temp->next;
        }
        cout << "Category ID " << id << " not found!\n";
    }

    // 6. DELETE: Remove node by ID
    void deleteCategory(int id)
    {
        if (head == nullptr)
        {
            cout << "Category list is empty!\n";
            return;
        }

        CategoryNode *temp = head;
        CategoryNode *prev = nullptr;

        while (temp != nullptr && temp->data.id != id)
        {
            prev = temp;
            temp = temp->next;
        }

        if (temp == nullptr)
        {
            cout << "Category ID " << id << " not found!\n";
            return;
        }

        // Re-link pointers
        if (temp == head)
        {
            head = head->next;
            if (head == nullptr)
                tail = nullptr; // List became empty
        }
        else
        {
            prev->next = temp->next;
            if (temp == tail)
                tail = prev; // Update tail if deleting the last node
        }

        delete temp;
        cout << "Category ID " << id << " deleted successfully!\n";
    }

    // In categoryt.hpp / category.h inside class CategoryList
    int getCategoryCount() const
    {
        int count = 0;
        CategoryNode *current = head; // replace CategoryNode with your node struct/class name
        while (current != nullptr)
        {
            count++;
            current = current->next;
        }
        return count;
    }
};


#endif