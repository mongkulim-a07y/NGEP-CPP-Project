#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <iostream>
#include <string>
#include "type.h"
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

// This class does NOT read from cin. All input-gathering (prompts) lives in
// main.cpp (see promptAddCustomer / promptEditCustomer / promptDeleteCustomer),
// same as CategoryList. That way this class can be reused, tested, or driven
// by something other than a console (a GUI, a script, tests) without change,
// and callers can validate things (like a duplicate username) BEFORE any
// customer data gets created — which a prompt-inside-the-class couldn't do.
class CustomerList
{
    // =====================================================================
    // Data structure
    // =====================================================================
private:
    struct CustomerNode
    {
        Customer data;
        CustomerNode *next;
        CustomerNode(Customer value) : data(value), next(nullptr) {}
    };

    CustomerNode *head;
    CustomerNode *tail;

    // Single source of truth for the CSV path, so load/save can never
    // accidentally point at two different files.
    static constexpr const char *CSV_PATH = "CsvFile/customerList.csv";

    // =====================================================================
    // Private helpers
    // =====================================================================

    // Shared lookup used by findCustomer()/editCustomer()/deleteCustomer() so
    // the list traversal logic lives in exactly one place.
    CustomerNode *findNode(int id) const
    {
        CustomerNode *temp = head;
        while (temp != nullptr)
        {
            if (temp->data.id == id)
                return temp;
            temp = temp->next;
        }
        return nullptr;
    }

public:
    // =====================================================================
    // Construction / destruction
    // =====================================================================

    CustomerList() : head(nullptr), tail(nullptr)
    {
        loadCustomers();
    }

    ~CustomerList()
    {
        freeMemory();
    }

    // Prevent accidental copies (raw pointers would double-free otherwise)
    CustomerList(const CustomerList &) = delete;
    CustomerList &operator=(const CustomerList &) = delete;

    void freeMemory()
    {
        CustomerNode *current = head;
        while (current != nullptr)
        {
            CustomerNode *temp = current;
            current = current->next;
            delete temp;
        }
        head = nullptr;
        tail = nullptr;
    }

    // =====================================================================
    // File I/O
    // =====================================================================

    void loadCustomers()
    {
        ifstream myFile(CSV_PATH);
        if (!myFile.is_open())
            return;

        freeMemory();
        string line;

        while (getline(myFile, line))
        {
            if (line.empty())
                continue;

            stringstream iss(line);
            string idStr, name, phone;

            if (getline(iss, idStr, ',') &&
                getline(iss, name, ',') &&
                getline(iss, phone))
            {
                if (idStr == "ID" || idStr == "id")
                    continue; // Skip CSV header row

                try
                {
                    Customer c;
                    c.id = stoi(idStr);
                    c.name = name;
                    c.phone = phone;

                    CustomerNode *newNode = new CustomerNode(c);
                    if (head == nullptr)
                    {
                        head = newNode;
                        tail = newNode;
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
        myFile.close();
    }

    void saveCustomers()
    {
        ofstream myFile(CSV_PATH);
        if (!myFile.is_open())
        {
            cout << "[Error] Failed to open " << CSV_PATH << "\n";
            return;
        }

        myFile << "ID,Name,Phone\n"; // header row, same convention as categories.csv / users.csv

        CustomerNode *temp = head;
        while (temp != nullptr)
        {
            myFile << temp->data.id << ','
                   << temp->data.name << ','
                   << temp->data.phone << "\n";
            temp = temp->next;
        }
        myFile.close();
    }

    // =====================================================================
    // ID management
    // =====================================================================

    // Predicts the ID that the NEXT addCustomer() call should use, without
    // adding anything. The caller is responsible for actually passing this
    // ID into addCustomer() — see promptAddCustomer() in main.cpp.
    int getNextCustomerId() const
    {
        int newId = 1;
        CustomerNode *temp = head;
        while (temp != nullptr)
        {
            if (temp->data.id >= newId)
                newId = temp->data.id + 1;
            temp = temp->next;
        }
        return newId;
    }

    int getCustomerCount() const
    {
        int count = 0;
        CustomerNode *temp = head;
        while (temp != nullptr)
        {
            count++;
            temp = temp->next;
        }
        return count;
    }

    // =====================================================================
    // Lookup
    // =====================================================================

    // Returns a pointer to the live customer record, or nullptr if not
    // found. Lets callers (e.g. an "edit" prompt) show current details
    // before asking for new ones.
    Customer *findCustomer(int id) const
    {
        CustomerNode *node = findNode(id);
        return node ? &node->data : nullptr;
    }

    // =====================================================================
    // CRUD operations (pure data - no cin, no cout prompts)
    // =====================================================================

    // Adds a new customer with the given data and saves to disk.
    // The caller supplies the id (typically from getNextCustomerId()).
    void addCustomer(int id, const string &name, const string &phone)
    {
        Customer c{id, name, phone};
        CustomerNode *newNode = new CustomerNode(c);

        if (head == nullptr)
        {
            head = newNode;
            tail = newNode;
        }
        else
        {
            tail->next = newNode;
            tail = newNode;
        }

        saveCustomers();
    }

    // Returns true if the customer was found and updated, false otherwise.
    bool editCustomer(int id, const string &newName, const string &newPhone)
    {
        CustomerNode *node = findNode(id);
        if (node == nullptr)
            return false;

        node->data.name = newName;
        node->data.phone = newPhone;
        saveCustomers();
        return true;
    }

    // Returns true if the customer was found and removed, false otherwise.
    bool deleteCustomer(int id)
    {
        CustomerNode *current = findNode(id);
        if (current == nullptr)
            return false;

        // Re-link neighbours (need the previous node, since it's a singly linked list)
        if (current == head)
        {
            head = head->next;
            if (head == nullptr)
                tail = nullptr;
        }
        else
        {
            CustomerNode *prev = head;
            while (prev->next != current)
                prev = prev->next;

            prev->next = current->next;
            if (current == tail)
                tail = prev;
        }

        delete current;
        saveCustomers();
        return true;
    }

    // =====================================================================
    // Display (pure output - reading the list back is not "input", so this
    // stays here just like CategoryList::viewCategories())
    // =====================================================================

    void viewCustomers() const
    {
        if (head == nullptr)
        {
            cout << "\n[Info] No customers found in the system.\n";
            return;
        }

        cout << "\n--------------------------------------------------" << endl;
        cout << setw(33) << "CUSTOMER DIRECTORY" << endl;
        cout << "--------------------------------------------------" << endl;
        cout << left << setw(10) << "ID"
             << setw(25) << "Full Name"
             << setw(15) << "Phone Number" << endl;
        cout << "--------------------------------------------------" << endl;

        int count = 0;
        CustomerNode *temp = head;
        while (temp != nullptr)
        {
            cout << left << setw(10) << temp->data.id
                 << setw(25) << temp->data.name
                 << setw(15) << temp->data.phone << endl;
            temp = temp->next;
            count++;
        }

        cout << "--------------------------------------------------" << endl;
        cout << "Total Customers: " << count << endl;
        cout << "--------------------------------------------------\n"
             << endl;
    }

    vector<Customer> getAllCustomers() const
    {
        vector<Customer> result;
        CustomerNode *temp = head;
        while (temp != nullptr)
        {
            result.push_back(temp->data);
            temp = temp->next;
        }
        return result;
    }
};

#endif