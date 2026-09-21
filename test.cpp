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

// promptAddProduct gathers input, calls the pure-data addProduct(), and
// prints the confirmation. Matches promptAddCategory/promptAddCustomer.
void promptAddProduct(ProductList &productApp)
{
    string name = getStringInput("Enter Product Name: ");
    int categoryId = getIntInput("Enter Category ID: ");
    double price = getDoubleInput("Enter Price: ");
    int stock = getIntInput("Enter Stock: ");

    int id = productApp.addProduct(name, categoryId, price, stock);
    cout << "Product added with id " << id << endl;
    productApp.saveProducts();
}

void promptEditProduct(ProductList &productApp)
{
    int id = getIntInput("Enter Product ID to edit: ");

    Product *existing = productApp.findProduct(id);
    if (existing == nullptr)
    {
        cout << "Product " << id << " not found" << endl;
        return;
    }

    string name = getStringInput("Enter New Name: ");
    int categoryId = getIntInput("Enter New Category ID: ");
    double price = getDoubleInput("Enter New Price: ");
    int stock = getIntInput("Enter New Stock: ");

    if (productApp.editProduct(id, name, categoryId, price, stock))
    {
        cout << "Product " << id << " updated" << endl;
        productApp.saveProducts();
    }
    else
    {
        cout << "Product " << id << " not found" << endl;
    }
}

void promptDeleteProduct(ProductList &productApp)
{
    int id = getIntInput("Enter Product ID to delete: ");

    if (productApp.deleteProduct(id))
    {
        cout << "Product " << id << " deleted" << endl;
        productApp.saveProducts();
    }
    else
    {
        cout << "Product " << id << " not found" << endl;
    }
}

// promptPlaceOrder gathers input, checks the product exists and has enough
// stock, and only THEN places the order and reduces stock. This is the fix
// for orders not syncing with product stock - previously OrderList::placeOrder
// took a product id/quantity straight from cin with no connection to
// ProductList at all, so it could "sell" a product that didn't exist or
// oversell stock with no check.
void promptPlaceOrder(OrderList &orderApp, ProductList &productApp, int customerId)
{
    int productId = getIntInput("Enter Product ID: ");
    int quantity = getIntInput("Enter Quantity: ");

    Product *prod = productApp.findProduct(productId);
    if (prod == nullptr)
    {
        cout << "[Error] Product ID " << productId << " not found.\n";
        return;
    }

    if (quantity <= 0)
    {
        cout << "[Error] Quantity must be greater than 0.\n";
        return;
    }

    if (quantity > prod->stock)
    {
        cout << "[Error] Insufficient stock. Available: " << prod->stock
             << ", Requested: " << quantity << ".\n";
        return;
    }

    int orderId = orderApp.placeOrder(customerId, productId, quantity);

    // Reducing stock and placing the order happen right next to each other
    // here (not hidden inside either class), so it's obvious they're always
    // done together.
    productApp.reduceStock(productId, quantity);
    productApp.saveProducts();

    cout << "[Success] Order ID " << orderId << " placed successfully for "
         << quantity << " x \"" << prod->name << "\".\n";
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

void runOwnerMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp, LoginList &users)
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
        cout << "  Login History\n";
        cout << " 17. View Login History\n";
        cout << "  0. Logout\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Choose (0-17): ");

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
            productApp.viewProducts();
            break;
        case 6:
            promptAddProduct(productApp);
            break;
        case 7:
            promptEditProduct(productApp);
            break;
        case 8:
            promptDeleteProduct(productApp);
            break;
        case 9:
            productApp.sortByPrice();
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
            promptPlaceOrder(orderApp, productApp, cid);
            break;
        }
        case 17:
            users.viewLoginHistory();
            break;
        case 0:
            cout << "Logging out...\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void runEmployeeMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp)
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
            productApp.viewProducts();
            break;
        case 2:
            productApp.sortByPrice();
            break;
        case 3:
            promptAddProduct(productApp);
            break;
        case 4:
            promptEditProduct(productApp);
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
            promptPlaceOrder(orderApp, productApp, cid);
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
void runCustomerMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp, int customerId)
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
            productApp.viewProducts();
            break;
        case 2:
            productApp.sortByPrice();
            break;
        case 3:
            category.viewCategories();
            break;
        case 4:
            promptPlaceOrder(orderApp, productApp, customerId);
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
void dispatchByRole(User *loggedInUser, CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp, LoginList &users)
{
    if (loggedInUser->tag == "admin")
        runOwnerMenu(customerApp, category, orderApp, productApp, users);
    else if (loggedInUser->tag == "staff")
        runEmployeeMenu(customerApp, category, orderApp, productApp);
    else if (loggedInUser->tag == "customer")
        runCustomerMenu(customerApp, category, orderApp, productApp, loggedInUser->id);
    else
        cout << "Unknown role. Please contact an administrator.\n";
}

void handleLogin(LoginList &users, CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp)
{
    string username = getStringInput("Username: ");
    string password = getStringInput("Password: ");

    User *loggedInUser = users.login(username, password); // prints its own success/failure message
    if (loggedInUser != nullptr)
        dispatchByRole(loggedInUser, customerApp, category, orderApp, productApp, users);
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
    LoginList users;        // manages CsvFile/users.csv (credentials + role)
    ProductList productApp; // manages CsvFile/product.csv (loads itself in its constructor)

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
            handleLogin(users, customerApp, category, orderApp, productApp);
            break;
        case 2:
            handleRegister(users, customerApp);
            break;
        case 3:
            productApp.saveProducts();
            // productApp's own destructor frees its memory automatically
            // when it goes out of scope below - no manual cleanup call needed.
            cout << "Goodbye!\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 3);

    return 0;
}