#include <iostream>
#include <limits>
#include <string>
#include <iomanip>

#include "type.h"
#include "customer.h"
#include "product.h"
// #include "category.h"
#include "order.h"
#include "category.hpp"
#include "loginlist.hpp"

using namespace std;

void clearInput()
{
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int getIntInput(const string &prompt)
{
    int val;
    while (true)
    {
        cout << prompt;
        if (cin >> val)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear trailing newline
            return val;
        }
        clearInput();
        cout << "Invalid input. Please enter a number.\n";
    }
}

double getDoubleInput(const string &prompt)
{
    double val;
    while (true)
    {
        cout << prompt;
        if (cin >> val)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear trailing newline
            return val;
        }
        clearInput();
        cout << "Invalid input. Please enter a number.\n";
    }
}

string getStringInput(const string &prompt)
{
    string str;
    cout << prompt;
    getline(cin, str);
    return str;
}

void promptAddProduct()
{
    string name = getStringInput("Enter Product Name: ");
    int categoryId = getIntInput("Enter Category ID: ");
    double price = getDoubleInput("Enter Price: ");
    int stock = getIntInput("Enter Stock: ");
    addProduct(name, categoryId, price, stock);
    saveProducts();
}

void promptEditProduct()
{
    int id = getIntInput("Enter Product ID to edit: ");
    string name = getStringInput("Enter New Name: ");
    int categoryId = getIntInput("Enter New Category ID: ");
    double price = getDoubleInput("Enter New Price: ");
    int stock = getIntInput("Enter New Stock: ");
    editProduct(id, name, categoryId, price, stock);
    saveProducts();
}

void promptDeleteProduct()
{
    int id = getIntInput("Enter Product ID to delete: ");
    deleteProduct(id);
    saveProducts();
}

void promptAddCategory(CategoryList &category)
{
    // Auto-generate ID based on current list size/count
    int id = category.getCategoryCount() + 1;

    cout << "Generated Category ID: " << id << "\n";
    string name = getStringInput("Enter Category Name: ");
    string desc = getStringInput("Enter Category Description: ");

    category.addCategory(id, name, desc);
    category.saveCategories();
}

void promptEditCategory(CategoryList &category)
{
    int id = getIntInput("Enter Category ID to edit: ");
    string name = getStringInput("Enter New Category Name: ");
    string desc = getStringInput("Enter New Category Description: ");
    category.editCategory(id, name, desc);
    category.saveCategories();
}

void promptDeleteCategory(CategoryList &category)
{
    int id = getIntInput("Enter Category ID to delete: ");
    category.deleteCategory(id);
    category.saveCategories();
}

// promptAddCustomer gathers input, calls the pure-data addCustomer(), and
// returns the new customer's ID so callers (e.g. handleRegister) can use it.
int promptAddCustomer(CustomerList &customerApp)
{
    int id = customerApp.getNextCustomerId();

    cout << "\n--------------------------------------------------\n";
    cout << setw(32) << "ADD NEW CUSTOMER" << "\n";
    cout << "--------------------------------------------------\n";
    cout << "Generated Customer ID: " << id << "\n";

    string name = getStringInput("Enter Customer Name: ");
    string phone = getStringInput("Enter Phone Number: ");

    customerApp.addCustomer(id, name, phone);
    cout << "[Success] Customer ID " << id << " added successfully.\n";
    return id;
}

void promptEditCustomer(CustomerList &customerApp)
{
    int id = getIntInput("Enter Customer ID to edit: ");

    Customer *existing = customerApp.findCustomer(id);
    if (existing == nullptr)
    {
        cout << "[Error] Customer ID " << id << " not found.\n";
        return;
    }

    cout << "\n[Current Details] Name: " << existing->name
         << " | Phone: " << existing->phone << "\n";

    string name = getStringInput("Enter New Name: ");
    string phone = getStringInput("Enter New Phone Number: ");

    if (customerApp.editCustomer(id, name, phone))
        cout << "[Success] Customer updated successfully.\n";
    else
        cout << "[Error] Customer ID " << id << " not found.\n";
}

void promptDeleteCustomer(CustomerList &customerApp)
{
    int id = getIntInput("Enter Customer ID to delete: ");

    if (customerApp.deleteCustomer(id))
        cout << "[Success] Customer ID " << id << " deleted successfully.\n";
    else
        cout << "[Error] Customer ID " << id << " not found.\n";
}

void runOwnerMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp)
{
    int choice;
    do
    {
        cout << "\n==================================================\n";
        cout << "                  OWNER MENU                      \n";
        cout << "==================================================\n";
        cout << "  Category\n";
        cout << "  1. View Categories\n";
        cout << "  2. Add Category\n";
        cout << "  3. Edit Category\n";
        cout << "  4. Delete Category\n";
        cout << "  Product\n";
        cout << "  5. View Products\n";
        cout << "  6. Add Product\n";
        cout << "  7. Edit Product\n";
        cout << "  8. Delete Product\n";
        cout << "  9. Sort Products by Price\n";
        cout << "  Customer\n";
        cout << " 10. View Customers\n";
        cout << " 11. Add Customer\n";
        cout << " 12. Edit Customer\n";
        cout << " 13. Delete Customer\n";
        cout << "  Order\n";
        cout << " 14. View All Orders\n";
        cout << " 15. View Orders by Customer\n";
        cout << " 16. Place Order\n";
        cout << "  0. Logout\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Choose (0-16): ");

        switch (choice)
        {
        case 1:
            category.viewCategories();
            break;
        case 2:
            promptAddCategory(category);
            break;
        case 3:
            promptEditCategory(category);
            break;
        case 4:
            promptDeleteCategory(category);
            break;
        case 5:
            viewProducts();
            break;
        case 6:
            promptAddProduct();
            break;
        case 7:
            promptEditProduct();
            break;
        case 8:
            promptDeleteProduct();
            break;
        case 9:
            sortByPrice();
            break;
        case 10:
            customerApp.viewCustomers();
            break;
        case 11:
            promptAddCustomer(customerApp);
            break;
        case 12:
            promptEditCustomer(customerApp);
            break;
        case 13:
            promptDeleteCustomer(customerApp);
            break;
        case 14:
            orderApp.viewAllOrders();
            break;
        case 15:
        {
            int cid = getIntInput("Enter Customer ID: ");
            orderApp.viewOrdersByCustomer(cid);
            break;
        }
        case 16:
        {
            int cid = getIntInput("Enter Customer ID: ");
            orderApp.placeOrder(cid);
            break;
        }
        case 0:
            cout << "Logging out...\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void runEmployeeMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp)
{
    int choice;
    do
    {
        cout << "\n==================================================\n";
        cout << "                 EMPLOYEE MENU                    \n";
        cout << "==================================================\n";
        cout << " 1. View Products\n";
        cout << " 2. Sort Products by Price\n";
        cout << " 3. Add Product\n";
        cout << " 4. Edit Product\n";
        cout << " 5. View Categories\n";
        cout << " 6. View Customers\n";
        cout << " 7. Add Customer\n";
        cout << " 8. Edit Customer\n";
        cout << " 9. Place Order\n";
        cout << "10. View All Orders\n";
        cout << "11. View Orders by Customer\n";
        cout << " 0. Logout\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Choose (0-11): ");

        switch (choice)
        {
        case 1:
            viewProducts();
            break;
        case 2:
            sortByPrice();
            break;
        case 3:
            promptAddProduct();
            break;
        case 4:
            promptEditProduct();
            break;
        case 5:
            category.viewCategories();
            break;
        case 6:
            customerApp.viewCustomers();
            break;
        case 7:
            promptAddCustomer(customerApp);
            break;
        case 8:
            promptEditCustomer(customerApp);
            break;
        case 9:
        {
            int cid = getIntInput("Enter Customer ID: ");
            orderApp.placeOrder(cid);
            break;
        }
        case 10:
            orderApp.viewAllOrders();
            break;
        case 11:
        {
            int cid = getIntInput("Enter Customer ID: ");
            orderApp.viewOrdersByCustomer(cid);
            break;
        }
        case 0:
            cout << "Logging out...\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// NOTE: now takes the logged-in customer's ID directly, instead of asking
// the customer to type their own ID at every step (they already proved who
// they are at login). "Register Account" was removed from here since
// registration now happens on the front screen, before login.
void runCustomerMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp, int customerId)
{
    int choice;
    do
    {
        cout << "\n==================================================\n";
        cout << "                 CUSTOMER MENU                    \n";
        cout << "==================================================\n";
        cout << " 1. View Products\n";
        cout << " 2. Sort Products by Price\n";
        cout << " 3. View Categories\n";
        cout << " 4. Place Order\n";
        cout << " 5. View My Orders\n";
        cout << " 0. Logout\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Choose (0-5): ");

        switch (choice)
        {
        case 1:
            viewProducts();
            break;
        case 2:
            sortByPrice();
            viewProducts();
            break;
        case 3:
            category.viewCategories();
            break;
        case 4:
            orderApp.placeOrder(customerId);
            break;
        case 5:
            orderApp.viewOrdersByCustomer(customerId);
            break;
        case 0:
            cout << "Logging out...\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// Routes a logged-in user to the correct menu based on their tag.
void dispatchByRole(User *loggedInUser, CustomerList &customerApp, CategoryList &category, OrderList &orderApp)
{
    if (loggedInUser->tag == "admin")
        runOwnerMenu(customerApp, category, orderApp);
    else if (loggedInUser->tag == "staff")
        runEmployeeMenu(customerApp, category, orderApp);
    else if (loggedInUser->tag == "customer")
        runCustomerMenu(customerApp, category, orderApp, loggedInUser->id);
    else
        cout << "Unknown role. Please contact an administrator.\n";
}

void handleLogin(LoginList &users, CustomerList &customerApp, CategoryList &category, OrderList &orderApp)
{
    string username = getStringInput("Username: ");
    string password = getStringInput("Password: ");

    User *loggedInUser = users.login(username, password); // prints its own success/failure message
    if (loggedInUser != nullptr)
        dispatchByRole(loggedInUser, customerApp, category, orderApp);
}

// Registration is customer-only here: admin/staff accounts are assumed to be
// provisioned separately (e.g. directly in users.csv, or by an owner-only
// "Add Staff" feature you can add later using LoginList::addUser(...)).
void handleRegister(LoginList &users, CustomerList &customerApp)
{
    cout << "\n--- Register New Customer Account ---\n";
    string username = getStringInput("Choose a Username: ");

    // Check FIRST, before creating anything. If we created the customer
    // record before this check (like before), a duplicate username would
    // leave an orphaned customer profile with no matching login.
    if (users.findUser(username) != nullptr)
    {
        cout << "[Error] Username \"" << username << "\" already exists. Registration cancelled.\n";
        return;
    }

    string password = getStringInput("Choose a Password: ");

    // Creates the customer profile and returns its new ID, so the login
    // record can be linked to the exact same customer with no manual
    // re-entry and no risk of the two files getting out of sync.
    int newCustomerId = promptAddCustomer(customerApp);

    if (users.addUser(username, password, "customer", newCustomerId))
        cout << "Registration complete! You can now log in.\n";
}

int main()
{
    CustomerList customerApp;
    OrderList orderApp;
    CategoryList category;
    LoginList users; // manages CsvFile/users.csv (credentials + role)
    loadProducts();

    int choice;
    do
    {
        cout << "\n==================================================\n";
        cout << "           STORE MANAGEMENT SYSTEM                \n";
        cout << "==================================================\n";
        cout << " 1. Login\n";
        cout << " 2. Register (Customer)\n";
        cout << " 3. Exit\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Select Option (1-3): ");

        switch (choice)
        {
        case 1:
            handleLogin(users, customerApp, category, orderApp);
            break;
        case 2:
            handleRegister(users, customerApp);
            break;
        case 3:
            saveProducts();
            freeAllProducts();
            cout << "Goodbye!\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 3);

    return 0;
}