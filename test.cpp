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

// Shared confirmation prompt for destructive actions (delete/cancel).
// Only "y" or "yes" (case-insensitive) counts as confirmed - anything
// else, including just pressing Enter, safely cancels.
bool confirmAction(const string &message)
{
    string response = getStringInput(message + " Type 'y' to confirm: ");
    return response == "y" || response == "Y" || response == "yes" || response == "Yes";
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

    Product *existing = productApp.findProduct(id);
    if (existing == nullptr)
    {
        cout << "Product " << id << " not found" << endl;
        return;
    }

    cout << "\n--- You are about to delete this product ---\n";
    cout << "ID: " << existing->id << "\n";
    cout << "Name: " << existing->name << "\n";
    cout << "Category ID: " << existing->categoryId << "\n";
    cout << "Price: $" << existing->price << "\n";
    cout << "Stock: " << existing->stock << "\n";
    cout << "----------------------------------------------\n";

    if (!confirmAction("This cannot be undone."))
    {
        cout << "Deletion cancelled.\n";
        return;
    }

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
// stock, and only THEN places the order and reduces stock.
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

    productApp.reduceStock(productId, quantity);
    productApp.saveProducts();

    cout << "[Success] Order ID " << orderId << " placed successfully for "
         << quantity << " x \"" << prod->name << "\".\n";
}

// Cancels an order and restores the stock it had taken.
void promptCancelOrder(OrderList &orderApp, ProductList &productApp)
{
    int orderId = getIntInput("Enter Order ID to cancel: ");

    Order *existing = orderApp.findOrder(orderId);
    if (existing == nullptr)
    {
        cout << "[Error] Order ID " << orderId << " not found.\n";
        return;
    }

    cout << "\n--- You are about to cancel this order ---\n";
    cout << "Order ID: " << existing->id << "\n";
    cout << "Customer ID: " << existing->customerId << "\n";
    cout << "Product ID: " << existing->productId << "\n";
    cout << "Quantity: " << existing->quantity << "\n";
    cout << "Date: " << existing->date << "\n";
    cout << "-------------------------------------------\n";

    if (!confirmAction("Stock will be restored, and this cannot be undone."))
    {
        cout << "Cancellation aborted.\n";
        return;
    }

    int productId, quantity;
    if (!orderApp.deleteOrder(orderId, productId, quantity))
    {
        cout << "[Error] Order ID " << orderId << " not found.\n";
        return;
    }

    Product *prod = productApp.findProduct(productId);
    if (prod != nullptr)
    {
        productApp.editProduct(productId, prod->name, prod->categoryId, prod->price, prod->stock + quantity);
        productApp.saveProducts();
    }

    cout << "[Success] Order ID " << orderId << " cancelled, stock restored.\n";
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

    Category *existing = category.findCategory(id);
    if (existing == nullptr)
    {
        cout << "Category ID " << id << " not found!\n";
        return;
    }

    cout << "\n--- You are about to delete this category ---\n";
    cout << "ID: " << existing->id << "\n";
    cout << "Name: " << existing->name << "\n";
    cout << "Description: " << existing->description << "\n";
    cout << "-----------------------------------------------\n";

    if (!confirmAction("This cannot be undone."))
    {
        cout << "Deletion cancelled.\n";
        return;
    }

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

    Customer *existing = customerApp.findCustomer(id);
    if (existing == nullptr)
    {
        cout << "[Error] Customer ID " << id << " not found.\n";
        return;
    }

    cout << "\n--- You are about to delete this customer ---\n";
    cout << "ID: " << existing->id << "\n";
    cout << "Name: " << existing->name << "\n";
    cout << "Phone: " << existing->phone << "\n";
    cout << "-----------------------------------------------\n";

    if (!confirmAction("This cannot be undone."))
    {
        cout << "Deletion cancelled.\n";
        return;
    }

    if (customerApp.deleteCustomer(id))
        cout << "[Success] Customer ID " << id << " deleted successfully.\n";
    else
        cout << "[Error] Customer ID " << id << " not found.\n";
}

// Lets a customer edit their OWN profile (never anyone else's - id comes
// from who they logged in as, not from typed input).
void promptEditOwnProfile(CustomerList &customerApp, int customerId)
{
    Customer *me = customerApp.findCustomer(customerId);
    if (me == nullptr)
    {
        cout << "[Error] Your customer profile could not be found.\n";
        return;
    }

    cout << "\n[Current Details] Name: " << me->name << " | Phone: " << me->phone << "\n";
    string name = getStringInput("Enter New Name: ");
    string phone = getStringInput("Enter New Phone Number: ");

    customerApp.editCustomer(customerId, name, phone);
    cout << "[Success] Profile updated.\n";
}

// Lets any logged-in user (owner/staff/customer) change their OWN password.
// Requires the current password as confirmation before allowing the change.
void promptChangePassword(LoginList &users, const string &username)
{
    string currentPassword = getStringInput("Enter Current Password: ");
    User *u = users.findUser(username);

    if (u == nullptr || u->password != currentPassword)
    {
        cout << "[Error] Current password is incorrect.\n";
        return;
    }

    string newPassword = getStringInput("Enter New Password: ");
    string confirmPassword = getStringInput("Confirm New Password: ");

    if (newPassword != confirmPassword)
    {
        cout << "[Error] Passwords do not match.\n";
        return;
    }

    users.editUser(username, newPassword, u->tag);
    cout << "[Success] Password changed successfully.\n";
}

// Owner-only: create a staff or admin login. Customers register themselves
// separately via handleRegister(); this is for provisioning employees.
void promptAddStaff(LoginList &users)
{
    string username = getStringInput("New Staff/Admin Username: ");
    if (users.findUser(username) != nullptr)
    {
        cout << "[Error] Username already exists.\n";
        return;
    }

    string password = getStringInput("Password: ");
    string role = getStringInput("Role (staff/admin): ");

    if (role != "staff" && role != "admin")
    {
        cout << "[Error] Role must be 'staff' or 'admin'.\n";
        return;
    }

    users.addUser(username, password, role, 0);
}

void runOwnerMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp, LoginList &users, const string &username)
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
        cout << " 17. Cancel Order\n";
        cout << "  Staff Accounts\n";
        cout << " 18. Add Staff/Admin Account\n";
        cout << "  Login History\n";
        cout << " 19. View Login History\n";
        cout << "  Account\n";
        cout << " 20. Change Password\n";
        cout << "  0. Logout\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Choose (0-20): ");

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
            promptCancelOrder(orderApp, productApp);
            break;
        case 18:
            promptAddStaff(users);
            break;
        case 19:
            users.viewLoginHistory();
            break;
        case 20:
            promptChangePassword(users, username);
            break;
        case 0:
            cout << "Logging out...\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void runEmployeeMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp, LoginList &users, const string &username)
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
        cout << "12. Change Password\n";
        cout << " 0. Logout\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Choose (0-12): ");

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
        case 12:
            promptChangePassword(users, username);
            break;
        case 0:
            cout << "Logging out...\n";
            break;
        default:
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// NOTE: takes the logged-in customer's ID directly, instead of asking the
// customer to type their own ID at every step.
void runCustomerMenu(CustomerList &customerApp, CategoryList &category, OrderList &orderApp, ProductList &productApp, LoginList &users, int customerId, const string &username)
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
        cout << " 6. Edit My Profile\n";
        cout << " 7. Change Password\n";
        cout << " 0. Logout\n";
        cout << "--------------------------------------------------\n";
        choice = getIntInput("Choose (0-7): ");

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
        case 6:
            promptEditOwnProfile(customerApp, customerId);
            break;
        case 7:
            promptChangePassword(users, username);
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
        runOwnerMenu(customerApp, category, orderApp, productApp, users, loggedInUser->username);
    else if (loggedInUser->tag == "staff")
        runEmployeeMenu(customerApp, category, orderApp, productApp, users, loggedInUser->username);
    else if (loggedInUser->tag == "customer")
        runCustomerMenu(customerApp, category, orderApp, productApp, users, loggedInUser->id, loggedInUser->username);
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

// Registration is customer-only here: admin/staff accounts are provisioned
// by an Owner instead, via promptAddStaff().
void handleRegister(LoginList &users, CustomerList &customerApp)
{
    cout << "\n--- Register New Customer Account ---\n";
    string username = getStringInput("Choose a Username: ");

    // Check FIRST, before creating anything. If we created the customer
    // record before this check, a duplicate username would leave an
    // orphaned customer profile with no matching login.
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