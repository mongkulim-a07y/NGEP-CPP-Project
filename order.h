#ifndef ORDER_H
#define ORDER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <string>
#include "type.h"

using namespace std;

// This class does NOT read from cin. All input-gathering AND the
// product/stock validation now live in main.cpp (see promptPlaceOrder()),
// because that validation needs both OrderList and ProductList together -
// it doesn't belong inside either class alone.
class OrderList
{
private:
    struct OrderNode
    {
        Order data;
        OrderNode *next;

        OrderNode(Order value) : data(value), next(nullptr) {}
    };

    OrderNode *orderHead;
    OrderNode *orderTail;

    static constexpr const char *CSV_PATH = "CsvFile/orderList.csv";

public:
    OrderList() : orderHead(nullptr), orderTail(nullptr)
    {
        loadOrders();
    }

    ~OrderList()
    {
        freeMemory();
    }

    // Prevent accidental copies (raw pointers would double-free otherwise)
    OrderList(const OrderList &) = delete;
    OrderList &operator=(const OrderList &) = delete;

    // getCurrentDateTime
    string getCurrentDateTime()
    {
        time_t now = time(0);
        tm *ltm = localtime(&now);

        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);

        return string(buffer);
    }

    // free order from memory
    void freeMemory()
    {
        OrderNode *current = orderHead;
        while (current != nullptr)
        {
            OrderNode *temp = current;
            current = current->next;
            delete temp;
        }
        orderHead = nullptr;
        orderTail = nullptr;
    }

    // load order from csv file into memory
    void loadOrders()
    {
        ifstream myFile(CSV_PATH);
        if (!myFile.is_open())
        {
            return;
        }

        freeMemory();
        string line;

        while (getline(myFile, line))
        {
            if (line.empty())
            {
                continue;
            }
            stringstream iss(line);
            string idStr, customerIdStr, productIdStr, quantityStr, date;

            if (getline(iss, idStr, ',') &&
                getline(iss, customerIdStr, ',') &&
                getline(iss, productIdStr, ',') &&
                getline(iss, quantityStr, ',') &&
                getline(iss, date))
            {
                if (idStr == "ID" || idStr == "id")
                {
                    continue; // Skip CSV header row
                }
                try
                {
                    Order od;
                    od.id = stoi(idStr);
                    od.customerId = stoi(customerIdStr);
                    od.productId = stoi(productIdStr);
                    od.quantity = stoi(quantityStr);
                    od.date = date;
                    OrderNode *newNode = new OrderNode(od);
                    if (orderHead == nullptr)
                    {
                        orderHead = newNode;
                        orderTail = newNode;
                    }
                    else
                    {
                        orderTail->next = newNode;
                        orderTail = newNode;
                    }
                }
                catch (...)
                {
                    continue;
                }
            }
        }
        myFile.close();
    }

    // save order into csv file
    void saveOrders()
    {
        ofstream myFile(CSV_PATH);
        if (!myFile.is_open())
        {
            cout << "[Error] Failed to open file\n";
            return;
        }

        myFile << "ID,CustomerID,ProductID,Quantity,Date\n"; // header row, same convention as the other CSVs

        OrderNode *temp = orderHead;
        while (temp != nullptr)
        {
            myFile << temp->data.id << ',' << temp->data.customerId
                   << ',' << temp->data.productId << ',' << temp->data.quantity << ',' << temp->data.date << "\n";
            temp = temp->next;
        }
        myFile.close();
        cout << "Orders saved successfully.\n";
    }

    // Predicts the ID the NEXT placeOrder() call would assign, without
    // adding anything - lets a caller show it ahead of time if it wants to.
    int getNextOrderId() const
    {
        int orderId = 1;
        OrderNode *temp = orderHead;
        while (temp != nullptr)
        {
            if (temp->data.id >= orderId)
            {
                orderId = temp->data.id + 1;
            }
            temp = temp->next;
        }
        return orderId;
    }

    // Places an order with the given data (pure data - no cin, and NO stock
    // check here, since that requires ProductList; see promptPlaceOrder()
    // in main.cpp, which validates the product/stock BEFORE calling this).
    // Returns the new order's id.
    int placeOrder(int customerId, int productId, int quantity)
    {
        int orderId = getNextOrderId();
        string date = getCurrentDateTime();

        Order o;
        o.id = orderId;
        o.customerId = customerId;
        o.productId = productId;
        o.quantity = quantity;
        o.date = date;
        OrderNode *newNode = new OrderNode(o);

        if (orderHead != nullptr)
        {
            orderTail->next = newNode;
            orderTail = newNode;
        }
        else
        {
            orderHead = newNode;
            orderTail = newNode;
        }

        saveOrders();
        return orderId;
    }

    // view all orders from csv file
    void viewAllOrders() const
    {
        if (orderHead == nullptr)
        {
            cout << "\n[Info] No orders found in the system.\n";
            return;
        }

        cout << "\n----------------------------------------------------------------------" << endl;
        cout << setw(45) << "VIEW ALL ORDERS" << endl;
        cout << "----------------------------------------------------------------------" << endl;
        cout << left << setw(10) << "Order ID"
             << setw(13) << "CustomerID"
             << setw(13) << "ProductID"
             << setw(10) << "Quantity"
             << setw(20) << "Date & Time" << endl;
        cout << "----------------------------------------------------------------------" << endl;
        int totalCount = 0;
        OrderNode *temp = orderHead;
        while (temp != nullptr)
        {
            cout << left << setw(10) << temp->data.id
                 << setw(13) << temp->data.customerId
                 << setw(13) << temp->data.productId
                 << setw(10) << temp->data.quantity
                 << setw(20) << temp->data.date << endl;
            temp = temp->next;
            totalCount++;
        }
        cout << "----------------------------------------------------------------------" << endl;
        cout << "Total System Orders: " << totalCount << endl;
        cout << "----------------------------------------------------------------------\n"
             << endl;
    }

    // view orders by customerId
    void viewOrdersByCustomer(int customerId) const
    {
        if (orderHead == nullptr)
        {
            cout << "\n[Info] No Order Found!.\n";
            return;
        }
        bool found = false;
        int count = 0;
        OrderNode *temp = orderHead;
        cout << "\n----------------------------------------------------------------------" << endl;
        cout << setw(42) << "VIEW CUSTOMER ORDERS" << endl;
        cout << "Target Customer ID: " << customerId << endl;
        cout << "----------------------------------------------------------------------" << endl;
        cout << left << setw(10) << "Order ID"
             << setw(13) << "CustomerID"
             << setw(13) << "ProductID"
             << setw(10) << "Quantity"
             << setw(20) << "Date & Time" << endl;
        cout << "----------------------------------------------------------------------" << endl;

        while (temp != nullptr)
        {
            if (temp->data.customerId == customerId)
            {
                count++;
                found = true;
                cout << left << setw(10) << temp->data.id
                     << setw(13) << temp->data.customerId
                     << setw(13) << temp->data.productId
                     << setw(10) << temp->data.quantity
                     << setw(20) << temp->data.date << endl;
            }
            temp = temp->next;
        }

        if (!found)
        {
            cout << "\n[Info] No orders registered under Customer ID " << customerId << ".\n";
        }
        else
        {
            cout << "----------------------------------------------------------------------" << endl;
            cout << "Total Orders for Customer #" << customerId << ": " << count << endl;
        }
        cout << "----------------------------------------------------------------------" << endl;
    }
};

#endif