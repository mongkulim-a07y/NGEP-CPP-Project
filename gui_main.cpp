// gui_main.cpp
// GUI front end for the Store Management System, built with raylib + raygui.
//
// This file does NOT change any of your data classes' console behavior -
// it's a second front end sitting on top of the same LoginList / CategoryList
// / CustomerList / ProductList / OrderList classes used by the console
// main.cpp. Both front ends can coexist; this is a separate build target.
//
// SETUP: place raylib.h and raygui.h in this same folder (you said you
// already have them). Build with something like:
//   g++ -std=c++17 gui_main.cpp -o gui_app -lraylib -lopengl32 -lgdi32 -lwinmm   (Windows/MinGW)
//   g++ -std=c++17 gui_main.cpp -o gui_app -lraylib -lGL -lm -lpthread -ldl -lrt -lX11   (Linux)
// Exact linker flags depend on your raylib install - see raylib's own
// "build your project" docs if these don't match your setup.
//
// NOTE: LoginList::login(), CategoryList::addCategory(), etc. still print
// status messages to the console (via cout) as a side effect - that's
// harmless, just means a console window will show some text alongside the
// GUI window. That's existing behavior from the console app being reused
// here, not something new this file adds.

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

#include "type.h"
#include "loginlist.hpp"
#include "category.hpp"
#include "customer.h"
#include "product.h"
#include "order.h"

using namespace std;

// =============================================================================
// Small shared helpers
// =============================================================================

// Formats a double as a 2-decimal money string ("9.99") instead of
// to_string()'s default ("9.990000").
static string FormatMoney(double v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return string(buf);
}

// Copies a std::string into a fixed C buffer, always leaving room for the
// null terminator. Used when loading a record into an edit form's textboxes.
static void CopyIntoBuffer(char *dest, size_t destSize, const string &src)
{
    strncpy(dest, src.c_str(), destSize - 1);
    dest[destSize - 1] = '\0';
}

// A yes/no confirmation modal, reused by every section's Delete/Cancel
// button. Call DrawConfirmModal() once per frame; it returns 1 the frame
// "Yes" is clicked, 2 the frame "No"/close is clicked, 0 otherwise (either
// closed already, or still open with no decision yet this frame).
struct ConfirmState
{
    bool open = false;
    int targetId = -1;
    string message;
};

static int DrawConfirmModal(ConfirmState &cs)
{
    if (!cs.open)
        return 0;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    DrawRectangle(0, 0, screenW, screenH, Fade(RAYWHITE, 0.85f));

    Rectangle box = {(float)screenW / 2 - 200, (float)screenH / 2 - 70, 400, 140};
    int btnActive = -1;
    GuiMessageBox(box, "Confirm", cs.message.c_str(), "Yes;No", &btnActive);

    if (btnActive == 1)
    {
        cs.open = false;
        return 1; // "Yes"
    }
    if (btnActive == 0 || btnActive == 2)
    {
        cs.open = false;
        return 2; // closed via X, or "No"
    }
    return 0; // still open, no decision yet
}

// =============================================================================
// Screens / navigation state
// =============================================================================

enum AppScreen
{
    SCREEN_LOGIN,
    SCREEN_REGISTER,
    SCREEN_OWNER
};

enum OwnerTab
{
    TAB_CATEGORY,
    TAB_PRODUCT,
    TAB_CUSTOMER,
    TAB_ORDER
};

// What we remember about who's currently logged in. We store copies (not a
// User* into LoginList) so nothing can dangle if the list is ever mutated
// elsewhere while this session is active.
struct Session
{
    string username;
    string tag;
    int customerId = 0;
};

// =============================================================================
// Owner Dashboard - Category section
// =============================================================================

static void DrawCategorySection(Rectangle area, CategoryList &categories)
{
    enum
    {
        FIELD_NAME,
        FIELD_DESC
    };
    static int activeField = -1;

    static char nameBuf[64] = "";
    static char descBuf[128] = "";
    static int selectedIndex = -1;
    static int scrollIndex = 0;
    static int editingId = -1; // -1 = "Add" mode, otherwise the id being edited
    static string statusMsg;
    static ConfirmState confirm;

    vector<Category> items = categories.getAllCategories();

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";
        listStr += "#" + to_string(items[i].id) + " " + items[i].name;
    }
    if (listStr.empty())
        listStr = "No categories yet";

    float x = area.x, y = area.y, w = area.width;
    GuiLabel({x, y, w, 24}, "Categories");
    y += 30;

    Rectangle listRect = {x, y, w * 0.5f, area.height - 60};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &selectedIndex);

    float formX = x + listRect.width + 20;
    float formW = w - listRect.width - 20;
    float fy = y;

    GuiLabel({formX, fy, formW, 18}, "Name:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, nameBuf, sizeof(nameBuf), activeField == FIELD_NAME))
        activeField = (activeField == FIELD_NAME) ? -1 : FIELD_NAME;
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Description:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, descBuf, sizeof(descBuf), activeField == FIELD_DESC))
        activeField = (activeField == FIELD_DESC) ? -1 : FIELD_DESC;
    fy += 38;

    const char *addLabel = (editingId == -1) ? "Add" : "Update";
    if (GuiButton({formX, fy, formW / 2 - 5, 32}, addLabel))
    {
        string name(nameBuf), desc(descBuf);
        if (name.empty())
        {
            statusMsg = "Name cannot be empty.";
        }
        else if (editingId == -1)
        {
            int newId = categories.getCategoryCount() + 1;
            categories.addCategory(newId, name, desc);
            categories.saveCategories();
            statusMsg = "Added category #" + to_string(newId);
            nameBuf[0] = '\0';
            descBuf[0] = '\0';
        }
        else
        {
            categories.editCategory(editingId, name, desc);
            categories.saveCategories();
            statusMsg = "Updated category #" + to_string(editingId);
            editingId = -1;
            nameBuf[0] = '\0';
            descBuf[0] = '\0';
        }
    }
    if (GuiButton({formX + formW / 2 + 5, fy, formW / 2 - 5, 32}, "Clear"))
    {
        nameBuf[0] = '\0';
        descBuf[0] = '\0';
        editingId = -1;
    }
    fy += 40;

    if (selectedIndex >= 0 && selectedIndex < (int)items.size())
    {
        if (GuiButton({formX, fy, formW / 2 - 5, 32}, "Load for Edit"))
        {
            const Category &c = items[selectedIndex];
            editingId = c.id;
            CopyIntoBuffer(nameBuf, sizeof(nameBuf), c.name);
            CopyIntoBuffer(descBuf, sizeof(descBuf), c.description);
        }
        if (GuiButton({formX + formW / 2 + 5, fy, formW / 2 - 5, 32}, "Delete"))
        {
            confirm.open = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Delete category \"" + items[selectedIndex].name + "\"?";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (DrawConfirmModal(confirm) == 1)
    {
        categories.deleteCategory(confirm.targetId);
        categories.saveCategories();
        selectedIndex = -1;
        editingId = -1;
        statusMsg = "Category deleted.";
    }
}

// =============================================================================
// Owner Dashboard - Product section
// =============================================================================

static void DrawProductSection(Rectangle area, ProductList &products)
{
    enum
    {
        FIELD_NAME,
        FIELD_CAT,
        FIELD_PRICE,
        FIELD_STOCK
    };
    static int activeField = -1;

    static char nameBuf[64] = "";
    static char catBuf[16] = "";
    static char priceBuf[16] = "";
    static char stockBuf[16] = "";
    static int selectedIndex = -1;
    static int scrollIndex = 0;
    static int editingId = -1;
    static string statusMsg;
    static ConfirmState confirm;

    vector<Product> items = products.getAllProducts();

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";
        listStr += "#" + to_string(items[i].id) + " " + items[i].name +
                   " ($" + FormatMoney(items[i].price) + ", stock " + to_string(items[i].stock) + ")";
    }
    if (listStr.empty())
        listStr = "No products yet";

    float x = area.x, y = area.y, w = area.width;
    GuiLabel({x, y, w, 24}, "Products");
    y += 30;

    Rectangle listRect = {x, y, w * 0.5f, area.height - 60};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &selectedIndex);

    float formX = x + listRect.width + 20;
    float formW = w - listRect.width - 20;
    float fy = y;

    GuiLabel({formX, fy, formW, 18}, "Name:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, nameBuf, sizeof(nameBuf), activeField == FIELD_NAME))
        activeField = (activeField == FIELD_NAME) ? -1 : FIELD_NAME;
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Category ID:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, catBuf, sizeof(catBuf), activeField == FIELD_CAT))
        activeField = (activeField == FIELD_CAT) ? -1 : FIELD_CAT;
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Price:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, priceBuf, sizeof(priceBuf), activeField == FIELD_PRICE))
        activeField = (activeField == FIELD_PRICE) ? -1 : FIELD_PRICE;
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Stock:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, stockBuf, sizeof(stockBuf), activeField == FIELD_STOCK))
        activeField = (activeField == FIELD_STOCK) ? -1 : FIELD_STOCK;
    fy += 38;

    const char *addLabel = (editingId == -1) ? "Add" : "Update";
    if (GuiButton({formX, fy, formW / 2 - 5, 32}, addLabel))
    {
        try
        {
            string name(nameBuf);
            int categoryId = stoi(string(catBuf));
            double price = stod(string(priceBuf));
            int stock = stoi(string(stockBuf));

            if (name.empty())
            {
                statusMsg = "Name cannot be empty.";
            }
            else if (editingId == -1)
            {
                int newId = products.addProduct(name, categoryId, price, stock);
                products.saveProducts();
                statusMsg = "Added product #" + to_string(newId);
                nameBuf[0] = '\0';
                catBuf[0] = '\0';
                priceBuf[0] = '\0';
                stockBuf[0] = '\0';
            }
            else
            {
                products.editProduct(editingId, name, categoryId, price, stock);
                products.saveProducts();
                statusMsg = "Updated product #" + to_string(editingId);
                editingId = -1;
                nameBuf[0] = '\0';
                catBuf[0] = '\0';
                priceBuf[0] = '\0';
                stockBuf[0] = '\0';
            }
        }
        catch (...)
        {
            statusMsg = "Category ID / Price / Stock must be valid numbers.";
        }
    }
    if (GuiButton({formX + formW / 2 + 5, fy, formW / 2 - 5, 32}, "Clear"))
    {
        nameBuf[0] = '\0';
        catBuf[0] = '\0';
        priceBuf[0] = '\0';
        stockBuf[0] = '\0';
        editingId = -1;
    }
    fy += 40;

    if (selectedIndex >= 0 && selectedIndex < (int)items.size())
    {
        if (GuiButton({formX, fy, formW / 2 - 5, 32}, "Load for Edit"))
        {
            const Product &p = items[selectedIndex];
            editingId = p.id;
            CopyIntoBuffer(nameBuf, sizeof(nameBuf), p.name);
            snprintf(catBuf, sizeof(catBuf), "%d", p.categoryId);
            snprintf(priceBuf, sizeof(priceBuf), "%.2f", p.price);
            snprintf(stockBuf, sizeof(stockBuf), "%d", p.stock);
        }
        if (GuiButton({formX + formW / 2 + 5, fy, formW / 2 - 5, 32}, "Delete"))
        {
            confirm.open = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Delete product \"" + items[selectedIndex].name + "\"?";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (DrawConfirmModal(confirm) == 1)
    {
        products.deleteProduct(confirm.targetId);
        products.saveProducts();
        selectedIndex = -1;
        editingId = -1;
        statusMsg = "Product deleted.";
    }
}

// =============================================================================
// Owner Dashboard - Customer section
// =============================================================================

static void DrawCustomerSection(Rectangle area, CustomerList &customers, LoginList &users)
{
    enum
    {
        FIELD_NAME,
        FIELD_PHONE
    };
    static int activeField = -1;

    static char nameBuf[64] = "";
    static char phoneBuf[32] = "";
    static int selectedIndex = -1;
    static int scrollIndex = 0;
    static int editingId = -1;
    static string statusMsg;
    static ConfirmState confirm;

    vector<Customer> items = customers.getAllCustomers();

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";
        listStr += "#" + to_string(items[i].id) + " " + items[i].name + " (" + items[i].phone + ")";
    }
    if (listStr.empty())
        listStr = "No customers yet";

    float x = area.x, y = area.y, w = area.width;
    GuiLabel({x, y, w, 24}, "Customers");
    y += 30;

    Rectangle listRect = {x, y, w * 0.5f, area.height - 60};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &selectedIndex);

    float formX = x + listRect.width + 20;
    float formW = w - listRect.width - 20;
    float fy = y;

    GuiLabel({formX, fy, formW, 18}, "Name:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, nameBuf, sizeof(nameBuf), activeField == FIELD_NAME))
        activeField = (activeField == FIELD_NAME) ? -1 : FIELD_NAME;
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Phone:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, phoneBuf, sizeof(phoneBuf), activeField == FIELD_PHONE))
        activeField = (activeField == FIELD_PHONE) ? -1 : FIELD_PHONE;
    fy += 38;

    const char *addLabel = (editingId == -1) ? "Add" : "Update";
    if (GuiButton({formX, fy, formW / 2 - 5, 32}, addLabel))
    {
        string name(nameBuf), phone(phoneBuf);
        if (name.empty() || phone.empty())
        {
            statusMsg = "Name and phone are required.";
        }
        else if (editingId == -1)
        {
            int newId = customers.getNextCustomerId();
            customers.addCustomer(newId, name, phone);
            statusMsg = "Added customer #" + to_string(newId);
            nameBuf[0] = '\0';
            phoneBuf[0] = '\0';
        }
        else
        {
            customers.editCustomer(editingId, name, phone);
            statusMsg = "Updated customer #" + to_string(editingId);
            editingId = -1;
            nameBuf[0] = '\0';
            phoneBuf[0] = '\0';
        }
    }
    if (GuiButton({formX + formW / 2 + 5, fy, formW / 2 - 5, 32}, "Clear"))
    {
        nameBuf[0] = '\0';
        phoneBuf[0] = '\0';
        editingId = -1;
    }
    fy += 40;

    if (selectedIndex >= 0 && selectedIndex < (int)items.size())
    {
        if (GuiButton({formX, fy, formW / 2 - 5, 32}, "Load for Edit"))
        {
            const Customer &c = items[selectedIndex];
            editingId = c.id;
            CopyIntoBuffer(nameBuf, sizeof(nameBuf), c.name);
            CopyIntoBuffer(phoneBuf, sizeof(phoneBuf), c.phone);
        }
        if (GuiButton({formX + formW / 2 + 5, fy, formW / 2 - 5, 32}, "Delete"))
        {
            confirm.open = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Delete customer \"" + items[selectedIndex].name +
                               "\"?\nTheir login account, if any, will also be removed.";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (DrawConfirmModal(confirm) == 1)
    {
        // Cascade-delete the matching login too, same fix as the console app.
        User *matchingLogin = users.findUserByCustomerId(confirm.targetId);
        customers.deleteCustomer(confirm.targetId);
        if (matchingLogin != nullptr)
        {
            string uname = matchingLogin->username; // copy before deleteUser frees the node
            users.deleteUser(uname);
        }
        selectedIndex = -1;
        editingId = -1;
        statusMsg = "Customer deleted.";
    }
}

// =============================================================================
// Owner Dashboard - Order section
// =============================================================================

static void DrawOrderSection(Rectangle area, OrderList &orders, ProductList &products)
{
    enum
    {
        FIELD_CUSTOMER,
        FIELD_PRODUCT,
        FIELD_QTY
    };
    static int activeField = -1;

    static char customerIdBuf[16] = "";
    static char productIdBuf[16] = "";
    static char quantityBuf[16] = "";
    static int selectedIndex = -1;
    static int scrollIndex = 0;
    static string statusMsg;
    static ConfirmState confirm;

    vector<Order> items = orders.getAllOrders();

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";
        listStr += "#" + to_string(items[i].id) + " Cust " + to_string(items[i].customerId) +
                   " - Prod " + to_string(items[i].productId) + " x" + to_string(items[i].quantity) +
                   " (" + items[i].date + ")";
    }
    if (listStr.empty())
        listStr = "No orders yet";

    float x = area.x, y = area.y, w = area.width;
    GuiLabel({x, y, w, 24}, "Orders");
    y += 30;

    Rectangle listRect = {x, y, w * 0.5f, area.height - 60};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &selectedIndex);

    float formX = x + listRect.width + 20;
    float formW = w - listRect.width - 20;
    float fy = y;

    GuiLabel({formX, fy, formW, 18}, "Place New Order");
    fy += 24;

    GuiLabel({formX, fy, formW, 18}, "Customer ID:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, customerIdBuf, sizeof(customerIdBuf), activeField == FIELD_CUSTOMER))
        activeField = (activeField == FIELD_CUSTOMER) ? -1 : FIELD_CUSTOMER;
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Product ID:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, productIdBuf, sizeof(productIdBuf), activeField == FIELD_PRODUCT))
        activeField = (activeField == FIELD_PRODUCT) ? -1 : FIELD_PRODUCT;
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Quantity:");
    fy += 20;
    if (GuiTextBox({formX, fy, formW, 28}, quantityBuf, sizeof(quantityBuf), activeField == FIELD_QTY))
        activeField = (activeField == FIELD_QTY) ? -1 : FIELD_QTY;
    fy += 38;

    if (GuiButton({formX, fy, formW, 32}, "Place Order"))
    {
        try
        {
            int customerId = stoi(string(customerIdBuf));
            int productId = stoi(string(productIdBuf));
            int quantity = stoi(string(quantityBuf));

            Product *prod = products.findProduct(productId);
            if (prod == nullptr)
            {
                statusMsg = "Product ID " + to_string(productId) + " not found.";
            }
            else if (quantity <= 0)
            {
                statusMsg = "Quantity must be greater than 0.";
            }
            else if (quantity > prod->stock)
            {
                statusMsg = "Insufficient stock. Available: " + to_string(prod->stock) +
                            ", Requested: " + to_string(quantity) + ".";
            }
            else
            {
                int orderId = orders.placeOrder(customerId, productId, quantity);
                products.reduceStock(productId, quantity);
                products.saveProducts();
                statusMsg = "Order #" + to_string(orderId) + " placed for " +
                            to_string(quantity) + " x \"" + prod->name + "\".";
                customerIdBuf[0] = '\0';
                productIdBuf[0] = '\0';
                quantityBuf[0] = '\0';
            }
        }
        catch (...)
        {
            statusMsg = "Customer ID / Product ID / Quantity must be valid numbers.";
        }
    }
    fy += 44;

    if (selectedIndex >= 0 && selectedIndex < (int)items.size())
    {
        if (GuiButton({formX, fy, formW, 32}, "Cancel Selected Order"))
        {
            confirm.open = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Cancel order #" + to_string(items[selectedIndex].id) +
                               "?\nStock will be restored.";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (DrawConfirmModal(confirm) == 1)
    {
        int productId, quantity;
        if (orders.deleteOrder(confirm.targetId, productId, quantity))
        {
            Product *prod = products.findProduct(productId);
            if (prod != nullptr)
            {
                products.editProduct(productId, prod->name, prod->categoryId, prod->price, prod->stock + quantity);
                products.saveProducts();
            }
            statusMsg = "Order cancelled, stock restored.";
        }
        selectedIndex = -1;
    }
}

// =============================================================================
// Owner Dashboard shell (sidebar + routes to the section above)
// =============================================================================

static void DrawOwnerDashboard(AppScreen &screen, OwnerTab &tab,
                                CategoryList &categories, ProductList &products,
                                CustomerList &customers, OrderList &orders,
                                LoginList &users, Session &session)
{
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float sidebarW = 190;

    GuiPanel({0, 0, sidebarW, (float)screenH}, "Owner Menu");

    float by = 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Categories"))
        tab = TAB_CATEGORY;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Products"))
        tab = TAB_PRODUCT;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Customers"))
        tab = TAB_CUSTOMER;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Orders"))
        tab = TAB_ORDER;

    by = (float)screenH - 80;
    GuiLabel({10, by, sidebarW - 20, 20}, ("User: " + session.username).c_str());
    by += 24;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Logout"))
    {
        screen = SCREEN_LOGIN;
        session.username.clear();
        session.tag.clear();
        session.customerId = 0;
    }

    Rectangle content = {sidebarW + 20, 20, (float)screenW - sidebarW - 40, (float)screenH - 40};

    switch (tab)
    {
    case TAB_CATEGORY:
        DrawCategorySection(content, categories);
        break;
    case TAB_PRODUCT:
        DrawProductSection(content, products);
        break;
    case TAB_CUSTOMER:
        DrawCustomerSection(content, customers, users);
        break;
    case TAB_ORDER:
        DrawOrderSection(content, orders, products);
        break;
    }
}

// =============================================================================
// Login screen
// =============================================================================

static void DrawLoginScreen(AppScreen &screen, LoginList &users, Session &session)
{
    enum
    {
        FIELD_USER,
        FIELD_PASS
    };
    static int activeField = -1;

    static char userBuf[64] = "";
    static char passBuf[64] = "";
    static string statusMsg;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float boxW = 340, boxH = 260;
    float bx = (float)screenW / 2 - boxW / 2, by = (float)screenH / 2 - boxH / 2;

    GuiPanel({bx, by, boxW, boxH}, "Store Management System - Login");

    float fx = bx + 20, fy = by + 40, fw = boxW - 40;

    GuiLabel({fx, fy, fw, 20}, "Username:");
    fy += 22;
    if (GuiTextBox({fx, fy, fw, 30}, userBuf, sizeof(userBuf), activeField == FIELD_USER))
        activeField = (activeField == FIELD_USER) ? -1 : FIELD_USER;
    fy += 40;

    GuiLabel({fx, fy, fw, 20}, "Password:");
    fy += 22;
    if (GuiTextBox({fx, fy, fw, 30}, passBuf, sizeof(passBuf), activeField == FIELD_PASS))
        activeField = (activeField == FIELD_PASS) ? -1 : FIELD_PASS;
    fy += 40;

    if (GuiButton({fx, fy, fw, 32}, "Login"))
    {
        User *u = users.login(string(userBuf), string(passBuf));
        if (u != nullptr)
        {
            session.username = u->username;
            session.tag = u->tag;
            session.customerId = u->id;
            userBuf[0] = '\0';
            passBuf[0] = '\0';

            if (u->tag == "admin")
            {
                statusMsg = "";
                screen = SCREEN_OWNER;
            }
            else
            {
                // Only the Owner dashboard exists in this first GUI pass.
                statusMsg = "Logged in as \"" + u->tag + "\" - only the Owner dashboard is built so far.";
            }
        }
        else
        {
            statusMsg = "Login failed - check username/password.";
        }
    }
    fy += 40;

    if (GuiButton({fx, fy, fw, 28}, "Register (Customer)"))
    {
        screen = SCREEN_REGISTER;
        statusMsg = "";
    }
    fy += 36;

    if (!statusMsg.empty())
        GuiLabel({fx, fy, fw, 40}, statusMsg.c_str());
}

// =============================================================================
// Register screen
// =============================================================================

static void DrawRegisterScreen(AppScreen &screen, LoginList &users, CustomerList &customers)
{
    enum
    {
        FIELD_USER,
        FIELD_PASS,
        FIELD_CONFIRM,
        FIELD_NAME,
        FIELD_PHONE
    };
    static int activeField = -1;

    static char userBuf[64] = "";
    static char passBuf[64] = "";
    static char confirmBuf[64] = "";
    static char nameBuf[64] = "";
    static char phoneBuf[32] = "";
    static string statusMsg;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float boxW = 380, boxH = 430;
    float bx = (float)screenW / 2 - boxW / 2, by = (float)screenH / 2 - boxH / 2;

    GuiPanel({bx, by, boxW, boxH}, "Register New Customer Account");

    float fx = bx + 20, fy = by + 40, fw = boxW - 40;

    GuiLabel({fx, fy, fw, 18}, "Username:");
    fy += 20;
    if (GuiTextBox({fx, fy, fw, 28}, userBuf, sizeof(userBuf), activeField == FIELD_USER))
        activeField = (activeField == FIELD_USER) ? -1 : FIELD_USER;
    fy += 34;

    GuiLabel({fx, fy, fw, 18}, "Password:");
    fy += 20;
    if (GuiTextBox({fx, fy, fw, 28}, passBuf, sizeof(passBuf), activeField == FIELD_PASS))
        activeField = (activeField == FIELD_PASS) ? -1 : FIELD_PASS;
    fy += 34;

    GuiLabel({fx, fy, fw, 18}, "Confirm Password:");
    fy += 20;
    if (GuiTextBox({fx, fy, fw, 28}, confirmBuf, sizeof(confirmBuf), activeField == FIELD_CONFIRM))
        activeField = (activeField == FIELD_CONFIRM) ? -1 : FIELD_CONFIRM;
    fy += 34;

    GuiLabel({fx, fy, fw, 18}, "Full Name:");
    fy += 20;
    if (GuiTextBox({fx, fy, fw, 28}, nameBuf, sizeof(nameBuf), activeField == FIELD_NAME))
        activeField = (activeField == FIELD_NAME) ? -1 : FIELD_NAME;
    fy += 34;

    GuiLabel({fx, fy, fw, 18}, "Phone:");
    fy += 20;
    if (GuiTextBox({fx, fy, fw, 28}, phoneBuf, sizeof(phoneBuf), activeField == FIELD_PHONE))
        activeField = (activeField == FIELD_PHONE) ? -1 : FIELD_PHONE;
    fy += 40;

    if (GuiButton({fx, fy, fw / 2 - 5, 32}, "Register"))
    {
        string username(userBuf), password(passBuf), confirmPass(confirmBuf);
        string name(nameBuf), phone(phoneBuf);

        // Same order of checks as handleRegister() in the console app:
        // duplicate-username check BEFORE creating any customer record, so
        // a rejected registration never leaves an orphaned customer row.
        if (username.empty() || password.empty() || name.empty() || phone.empty())
        {
            statusMsg = "Please fill in all fields.";
        }
        else if (users.findUser(username) != nullptr)
        {
            statusMsg = "Username \"" + username + "\" already exists.";
        }
        else if (password != confirmPass)
        {
            statusMsg = "Passwords do not match.";
        }
        else
        {
            int newId = customers.getNextCustomerId();
            customers.addCustomer(newId, name, phone);
            users.addUser(username, password, "customer", newId);
            statusMsg = "Registered! You can now log in.";
            userBuf[0] = '\0';
            passBuf[0] = '\0';
            confirmBuf[0] = '\0';
            nameBuf[0] = '\0';
            phoneBuf[0] = '\0';
        }
    }
    if (GuiButton({fx + fw / 2 + 5, fy, fw / 2 - 5, 32}, "Back to Login"))
    {
        screen = SCREEN_LOGIN;
        statusMsg = "";
    }
    fy += 42;

    if (!statusMsg.empty())
        GuiLabel({fx, fy, fw, 40}, statusMsg.c_str());
}

// =============================================================================
// main
// =============================================================================

int main()
{
    const int screenWidth = 1200;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Store Management System");
    SetTargetFPS(60);

    // Same data classes the console app uses - loaded from the same
    // CsvFile/*.csv files, so this GUI and the console main.cpp share data
    // (just not at the same time, since each holds its own in-memory copy
    // while running).
    LoginList users;
    CategoryList categories;
    ProductList products;
    CustomerList customers;
    OrderList orders;

    AppScreen screen = SCREEN_LOGIN;
    OwnerTab tab = TAB_CATEGORY;
    Session session;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        switch (screen)
        {
        case SCREEN_LOGIN:
            DrawLoginScreen(screen, users, session);
            break;
        case SCREEN_REGISTER:
            DrawRegisterScreen(screen, users, customers);
            break;
        case SCREEN_OWNER:
            DrawOwnerDashboard(screen, tab, categories, products, customers, orders, users, session);
            break;
        }

        EndDrawing();
    }

    products.saveProducts();
    CloseWindow();
    return 0;
}
