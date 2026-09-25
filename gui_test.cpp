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

#include <algorithm>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cctype>

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

// A small "how many?" modal used by the staff Browse & Order screen: click
// a product row, hit "Order This", type a quantity, confirm. Call
// DrawQuantityPromptModal() once per frame; it returns 1 the frame
// "Confirm" is clicked (quantity is left in qp.quantityBuf for the caller
// to parse), 2 the frame "Cancel"/close is clicked, 0 otherwise.
struct QuantityPromptState
{
    bool open = false;
    int productId = -1;
    string productName;
    int availableStock = 0;
    char quantityBuf[16] = "";
};

static int DrawQuantityPromptModal(QuantityPromptState &qp)
{
    if (!qp.open)
        return 0;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    DrawRectangle(0, 0, screenW, screenH, Fade(RAYWHITE, 0.85f));

    Rectangle box = {(float)screenW / 2 - 180, (float)screenH / 2 - 90, 360, 180};
    GuiPanel(box, "Order This Product");

    float px = box.x + 20, py = box.y + 34, pw = box.width - 40;
    string label = qp.productName + " (stock: " + to_string(qp.availableStock) + ")";
    GuiLabel({px, py, pw, 20}, label.c_str());
    py += 26;

    GuiLabel({px, py, pw, 18}, "Quantity:");
    py += 20;
    GuiTextBox({px, py, pw, 28}, qp.quantityBuf, sizeof(qp.quantityBuf), true);
    py += 40;

    int result = 0;
    if (GuiButton({px, py, pw / 2 - 5, 32}, "Confirm"))
        result = 1;
    if (GuiButton({px + pw / 2 + 5, py, pw / 2 - 5, 32}, "Cancel"))
        result = 2;

    if (result != 0)
    {
        qp.open = false;
        if (result == 2)
            qp.quantityBuf[0] = '\0';
    }
    return result;
}

// =============================================================================
// Screens / navigation state
// =============================================================================

enum AppScreen
{
    SCREEN_LOGIN,
    SCREEN_REGISTER,
    SCREEN_OWNER,
    SCREEN_STAFF,
    SCREEN_CUSTOMER
};

enum OwnerTab
{
    TAB_CATEGORY,
    TAB_PRODUCT,
    TAB_CUSTOMER,
    TAB_ORDER,
    TAB_ACCOUNT
};

// Staff dashboard has its own, smaller tab set - staff can browse/order,
// look things up, and manage their own password, but can't delete
// customers, cancel orders, add other accounts, or see login history.
enum StaffTab
{
    STAFF_TAB_BROWSE_ORDER,
    STAFF_TAB_CATEGORY,
    STAFF_TAB_CUSTOMER,
    STAFF_TAB_ORDER,
    STAFF_TAB_ACCOUNT
};

enum CustomerTab
{
    CUSTOMER_TAB_BROWSE_ORDER,
    CUSTOMER_TAB_ORDERS,
    CUSTOMER_TAB_ACCOUNT
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
// Modal-open tracking, file scope
// =============================================================================
// Each section function owns its own ConfirmState/QuantityPromptState as a
// function-local static (so its state persists across frames without any
// extra plumbing) - but that means the dashboard functions, which draw the
// sidebar BEFORE calling into the active tab's section function, have no
// way to know a modal is about to be open on this frame and lock the
// sidebar accordingly. Confirm modals draw a dimmed overlay, but that's
// purely visual (DrawRectangle doesn't intercept clicks) - GuiLock() is
// what actually blocks input, and only for controls drawn AFTER it's
// called. So we track "is this section's modal currently open" here, at
// file scope, and the dashboard checks it before drawing its sidebar.
static bool g_categoryModalOpen = false;
static bool g_productModalOpen = false;
static bool g_customerModalOpen = false;
static bool g_orderModalOpen = false;
static bool g_staffOrderModalOpen = false;
static bool g_customerOrderModalOpen = false;

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

    g_categoryModalOpen = confirm.open;

    // Lock everything else while the delete confirmation is open, so a
    // click can't land on "Add"/"Load for Edit"/a different list row
    // through the dimmed overlay. Unlocked again right before the modal
    // itself draws, further down. (The dashboard has already locked the
    // sidebar for this frame too, via g_categoryModalOpen - see
    // DrawOwnerDashboard.)
    if (confirm.open)
        GuiLock();

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
            g_categoryModalOpen = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Delete category \"" + items[selectedIndex].name + "\"?";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (confirm.open)
        GuiUnlock();

    if (DrawConfirmModal(confirm) == 1)
    {
        categories.deleteCategory(confirm.targetId);
        categories.saveCategories();
        selectedIndex = -1;
        editingId = -1;
        statusMsg = "Category deleted.";
    }
    g_categoryModalOpen = confirm.open;
}

// =============================================================================
// Owner Dashboard - Product section
// =============================================================================

static void DrawProductSection(Rectangle area, ProductList &products, CategoryList &categories)
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

    // Category dropdown (picker) state - a convenience for filling catBuf,
    // not a separate source of truth. Whichever way the person sets catBuf
    // (typing or picking), the same text field is what gets parsed on
    // Add/Update.
    static bool categoryDropdownEditMode = false;
    static int categoryDropdownActive = -1;

    g_productModalOpen = confirm.open;

    // GuiDropdownBox's expanded list must be drawn AFTER every other
    // control that could otherwise cover it, so lock everything else while
    // it's open. We unlock again right before drawing the dropdown itself,
    // at the very end of this function. See raygui's own controls_test_suite
    // example, which uses this same Lock-draw-everything-else-then-Unlock
    // pattern around GuiDropdownBox.
    if (categoryDropdownEditMode || confirm.open)
        GuiLock();

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
    float catTextBoxW = formW * 0.55f;
    float catDropdownW = formW - catTextBoxW - 6;
    if (GuiTextBox({formX, fy, catTextBoxW, 28}, catBuf, sizeof(catBuf), activeField == FIELD_CAT))
        activeField = (activeField == FIELD_CAT) ? -1 : FIELD_CAT;
    // The dropdown itself is drawn later (see bottom of this function) so its
    // expanded list isn't covered by the buttons/labels drawn below it - but
    // its position is fixed here, right next to the manual text box.
    Rectangle categoryDropdownRect = {formX + catTextBoxW + 6, fy, catDropdownW, 28};
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
            g_productModalOpen = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Delete product \"" + items[selectedIndex].name + "\"?";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (confirm.open)
        GuiUnlock();

    if (DrawConfirmModal(confirm) == 1)
    {
        products.deleteProduct(confirm.targetId);
        products.saveProducts();
        selectedIndex = -1;
        editingId = -1;
        statusMsg = "Product deleted.";
    }
    g_productModalOpen = confirm.open;

    // Category dropdown, drawn LAST so its expanded list renders on top of
    // everything above (see the comment where categoryDropdownEditMode is
    // declared). Picking an item here just writes that category's id into
    // catBuf - the exact same field the manual text box edits - so both
    // ways of choosing a category stay in sync and Add/Update always reads
    // from one place.
    if (categoryDropdownEditMode)
        GuiUnlock();

    vector<Category> allCats = categories.getAllCategories();
    string dropdownItems;
    for (size_t i = 0; i < allCats.size(); i++)
    {
        if (i > 0)
            dropdownItems += ";";
        dropdownItems += to_string(allCats[i].id) + " - " + allCats[i].name;
    }
    if (dropdownItems.empty())
        dropdownItems = "No categories yet";

    int previousActive = categoryDropdownActive;
    if (GuiDropdownBox(categoryDropdownRect, dropdownItems.c_str(), &categoryDropdownActive, categoryDropdownEditMode))
        categoryDropdownEditMode = !categoryDropdownEditMode;

    if (categoryDropdownActive != previousActive &&
        categoryDropdownActive >= 0 && categoryDropdownActive < (int)allCats.size())
    {
        snprintf(catBuf, sizeof(catBuf), "%d", allCats[categoryDropdownActive].id);
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

    g_customerModalOpen = confirm.open;

    if (confirm.open)
        GuiLock();

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
            g_customerModalOpen = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Delete customer \"" + items[selectedIndex].name +
                              "\"?\nTheir login account, if any, will also be removed.";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (confirm.open)
        GuiUnlock();

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
    g_customerModalOpen = confirm.open;
}

// =============================================================================
// Owner Dashboard - Order section
// =============================================================================

static void DrawOrderSection(Rectangle area, OrderList &orders, ProductList &products, CustomerList &customers)
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

    // Customer/Product pickers (convenience for filling the same text
    // boxes above, not a separate source of truth) - same pattern as the
    // category dropdown in DrawProductSection.
    static bool customerDropdownEditMode = false;
    static int customerDropdownActive = -1;
    static bool productDropdownEditMode = false;
    static int productDropdownActive = -1;

    g_orderModalOpen = confirm.open;

    // Lock everything else while either dropdown is open, for the same
    // reason as the category dropdown - unlocked again right before both
    // are drawn, at the very end of this function.
    if (customerDropdownEditMode || productDropdownEditMode || confirm.open)
        GuiLock();

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
    float custTextBoxW = formW * 0.55f;
    float custDropdownW = formW - custTextBoxW - 6;
    if (GuiTextBox({formX, fy, custTextBoxW, 28}, customerIdBuf, sizeof(customerIdBuf), activeField == FIELD_CUSTOMER))
        activeField = (activeField == FIELD_CUSTOMER) ? -1 : FIELD_CUSTOMER;
    Rectangle customerDropdownRect = {formX + custTextBoxW + 6, fy, custDropdownW, 28};
    fy += 34;

    GuiLabel({formX, fy, formW, 18}, "Product ID:");
    fy += 20;
    float prodTextBoxW = formW * 0.55f;
    float prodDropdownW = formW - prodTextBoxW - 6;
    if (GuiTextBox({formX, fy, prodTextBoxW, 28}, productIdBuf, sizeof(productIdBuf), activeField == FIELD_PRODUCT))
        activeField = (activeField == FIELD_PRODUCT) ? -1 : FIELD_PRODUCT;
    Rectangle productDropdownRect = {formX + prodTextBoxW + 6, fy, prodDropdownW, 28};
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
            g_orderModalOpen = true;
            confirm.targetId = items[selectedIndex].id;
            confirm.message = "Cancel order #" + to_string(items[selectedIndex].id) +
                              "?\nStock will be restored.";
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());

    if (confirm.open)
        GuiUnlock();

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
    g_orderModalOpen = confirm.open;

    // Customer/Product pickers, drawn LAST so their expanded lists render
    // on top of everything above (see the note where the dropdown state is
    // declared). Picking an item writes that id into the matching text box
    // above - the same field Place Order reads from either way.
    if (customerDropdownEditMode || productDropdownEditMode)
        GuiUnlock();

    vector<Customer> allCustomers = customers.getAllCustomers();
    string customerItems;
    for (size_t i = 0; i < allCustomers.size(); i++)
    {
        if (i > 0)
            customerItems += ";";
        customerItems += to_string(allCustomers[i].id) + " - " + allCustomers[i].name;
    }
    if (customerItems.empty())
        customerItems = "No customers yet";

    int prevCustomerActive = customerDropdownActive;
    if (GuiDropdownBox(customerDropdownRect, customerItems.c_str(), &customerDropdownActive, customerDropdownEditMode))
    {
        customerDropdownEditMode = !customerDropdownEditMode;
        if (customerDropdownEditMode)
            productDropdownEditMode = false; // only one dropdown open at a time
    }
    if (customerDropdownActive != prevCustomerActive &&
        customerDropdownActive >= 0 && customerDropdownActive < (int)allCustomers.size())
    {
        snprintf(customerIdBuf, sizeof(customerIdBuf), "%d", allCustomers[customerDropdownActive].id);
    }

    // FIX: Customer's row sits ABOVE Product's row, so Customer's expanded
    // list drops down into the space Product's box occupies. Drawing
    // Product's box (even closed) while Customer is open makes it visually
    // cut into Customer's open list. Skip Product entirely for this one
    // frame instead - it reappears normally the instant Customer closes.
    if (!customerDropdownEditMode)
    {
        vector<Product> allProductsForDropdown = products.getAllProducts();
        string productItems;
        for (size_t i = 0; i < allProductsForDropdown.size(); i++)
        {
            if (i > 0)
                productItems += ";";
            productItems += to_string(allProductsForDropdown[i].id) + " - " + allProductsForDropdown[i].name;
        }
        if (productItems.empty())
            productItems = "No products yet";

        int prevProductActive = productDropdownActive;
        if (GuiDropdownBox(productDropdownRect, productItems.c_str(), &productDropdownActive, productDropdownEditMode))
        {
            productDropdownEditMode = !productDropdownEditMode;
            if (productDropdownEditMode)
                customerDropdownEditMode = false; // only one dropdown open at a time
        }
        if (productDropdownActive != prevProductActive &&
            productDropdownActive >= 0 && productDropdownActive < (int)allProductsForDropdown.size())
        {
            snprintf(productIdBuf, sizeof(productIdBuf), "%d", allProductsForDropdown[productDropdownActive].id);
        }
    }
}

// =============================================================================
// Owner Dashboard - Account section (Change Password / Add Staff / Login History)
// =============================================================================

static void DrawAccountSection(Rectangle area, LoginList &users, Session &session)
{
    enum
    {
        FIELD_CURRENT,
        FIELD_NEWPASS,
        FIELD_CONFIRM,
        FIELD_NEWUSER,
        FIELD_NEWSTAFFPASS
    };
    static int activeField = -1;

    static char currentPassBuf[64] = "";
    static char newPassBuf[64] = "";
    static char confirmPassBuf[64] = "";
    static string changePassMsg;

    static char newStaffUserBuf[64] = "";
    static char newStaffPassBuf[64] = "";
    static int roleActive = 0; // 0 = Staff, 1 = Admin
    static string addStaffMsg;

    static int historyScroll = 0;

    float x = area.x, y = area.y, w = area.width;

    // Two columns: left = Change Password + Add Staff (stacked), right = Login History.
    float leftW = w * 0.45f;
    float rightX = x + leftW + 20;
    float rightW = w - leftW - 20;

    // ---- Change Password ----
    float fy = y;
    GuiGroupBox({x, fy, leftW, 220}, "Change Password");
    float cpx = x + 15, cpy = fy + 20, cpw = leftW - 30;

    GuiLabel({cpx, cpy, cpw, 18}, "Current Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, currentPassBuf, sizeof(currentPassBuf), activeField == FIELD_CURRENT))
        activeField = (activeField == FIELD_CURRENT) ? -1 : FIELD_CURRENT;
    cpy += 32;

    GuiLabel({cpx, cpy, cpw, 18}, "New Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, newPassBuf, sizeof(newPassBuf), activeField == FIELD_NEWPASS))
        activeField = (activeField == FIELD_NEWPASS) ? -1 : FIELD_NEWPASS;
    cpy += 32;

    GuiLabel({cpx, cpy, cpw, 18}, "Confirm New Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, confirmPassBuf, sizeof(confirmPassBuf), activeField == FIELD_CONFIRM))
        activeField = (activeField == FIELD_CONFIRM) ? -1 : FIELD_CONFIRM;
    cpy += 34;

    if (GuiButton({cpx, cpy, cpw, 28}, "Update Password"))
    {
        User *u = users.findUser(session.username);
        string currentPass(currentPassBuf), newPass(newPassBuf), confirmPass(confirmPassBuf);

        if (u == nullptr || u->password != currentPass)
        {
            changePassMsg = "Current password is incorrect.";
        }
        else if (newPass.empty())
        {
            changePassMsg = "New password cannot be empty.";
        }
        else if (newPass != confirmPass)
        {
            changePassMsg = "New passwords do not match.";
        }
        else
        {
            users.editUser(session.username, newPass, u->tag);
            changePassMsg = "Password changed successfully.";
            currentPassBuf[0] = '\0';
            newPassBuf[0] = '\0';
            confirmPassBuf[0] = '\0';
        }
    }
    cpy += 32;
    if (!changePassMsg.empty())
        GuiLabel({cpx, cpy, cpw, 34}, changePassMsg.c_str());

    fy += 240;

    // ---- Add Staff/Admin Account ----
    GuiGroupBox({x, fy, leftW, 220}, "Add Staff / Admin Account");
    float asx = x + 15, asy = fy + 20, asw = leftW - 30;

    GuiLabel({asx, asy, asw, 18}, "Username:");
    asy += 20;
    if (GuiTextBox({asx, asy, asw, 26}, newStaffUserBuf, sizeof(newStaffUserBuf), activeField == FIELD_NEWUSER))
        activeField = (activeField == FIELD_NEWUSER) ? -1 : FIELD_NEWUSER;
    asy += 32;

    GuiLabel({asx, asy, asw, 18}, "Password:");
    asy += 20;
    if (GuiTextBox({asx, asy, asw, 26}, newStaffPassBuf, sizeof(newStaffPassBuf), activeField == FIELD_NEWSTAFFPASS))
        activeField = (activeField == FIELD_NEWSTAFFPASS) ? -1 : FIELD_NEWSTAFFPASS;
    asy += 32;

    GuiLabel({asx, asy, asw, 18}, "Role:");
    asy += 20;
    GuiToggleGroup({asx, asy, asw / 2 - 1, 26}, "Staff;Admin", &roleActive);
    asy += 34;

    if (GuiButton({asx, asy, asw, 28}, "Add Account"))
    {
        string username(newStaffUserBuf), password(newStaffPassBuf);
        string role = (roleActive == 0) ? "staff" : "admin";

        if (username.empty() || password.empty())
        {
            addStaffMsg = "Username and password are required.";
        }
        else if (users.findUser(username) != nullptr)
        {
            addStaffMsg = "Username already exists.";
        }
        else
        {
            users.addUser(username, password, role, 0);
            addStaffMsg = "Added " + role + " account \"" + username + "\".";
            newStaffUserBuf[0] = '\0';
            newStaffPassBuf[0] = '\0';
        }
    }
    asy += 32;
    if (!addStaffMsg.empty())
        GuiLabel({asx, asy, asw, 34}, addStaffMsg.c_str());

    // ---- Login History (right column) ----
    GuiLabel({rightX, y, rightW, 24}, "Login History");
    float historyY = y + 30;

    vector<LoginHistoryEntry> history = users.getLoginHistory();
    string historyStr;
    for (int i = (int)history.size() - 1; i >= 0; i--) // most recent first
    {
        if (!historyStr.empty())
            historyStr += ";";
        historyStr += history[i].username + " (" + history[i].tag + ") - " + history[i].datetime;
    }
    if (historyStr.empty())
        historyStr = "No login history yet";

    Rectangle historyRect = {rightX, historyY, rightW, area.height - 30};
    int dummyActive = -1; // read-only log - nothing happens on click, so no need to persist a selection
    GuiListView(historyRect, historyStr.c_str(), &historyScroll, &dummyActive);
}

static void DrawOwnerDashboard(AppScreen &screen, OwnerTab &tab,
                               CategoryList &categories, ProductList &products,
                               CustomerList &customers, OrderList &orders,
                               LoginList &users, Session &session)
{
    // Always start each frame unlocked, regardless of whether a dropdown
    // was left open in a previous frame/section - otherwise a lock left on
    // from, say, the Product tab's category dropdown could make the
    // sidebar's tab buttons unresponsive after switching away from it.
    GuiUnlock();

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float sidebarW = 190;

    // A section's Delete/Cancel confirmation draws a dimmed overlay over
    // the WHOLE window, including the sidebar - but that overlay is purely
    // visual. Unless we lock the sidebar too, its buttons stay clickable
    // right through the dim, so "Logout" or switching tabs would work
    // while a pending delete/cancel is still on screen. Check the flag the
    // active tab's section function set on the LAST frame before drawing
    // the sidebar this frame - by the time that section function runs
    // again below, it'll re-lock/unlock around its own controls and the
    // modal as before; this just extends the same lock to the sidebar.
    bool modalOpenForActiveTab = false;
    switch (tab)
    {
    case TAB_CATEGORY:
        modalOpenForActiveTab = g_categoryModalOpen;
        break;
    case TAB_PRODUCT:
        modalOpenForActiveTab = g_productModalOpen;
        break;
    case TAB_CUSTOMER:
        modalOpenForActiveTab = g_customerModalOpen;
        break;
    case TAB_ORDER:
        modalOpenForActiveTab = g_orderModalOpen;
        break;
    case TAB_ACCOUNT:
        modalOpenForActiveTab = false; // Account tab has no confirm modal
        break;
    }
    if (modalOpenForActiveTab)
        GuiLock();

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
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Account"))
        tab = TAB_ACCOUNT;

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

    // The active tab's own section function manages GuiLock/GuiUnlock
    // around its own controls and modal as before - the sidebar lock set
    // above is independent of that and simply stays in effect for
    // whatever was already drawn (the sidebar), regardless of what the
    // section function does with the lock afterward.
    Rectangle content = {sidebarW + 20, 20, (float)screenW - sidebarW - 40, (float)screenH - 40};

    switch (tab)
    {
    case TAB_CATEGORY:
        DrawCategorySection(content, categories);
        break;
    case TAB_PRODUCT:
        DrawProductSection(content, products, categories);
        break;
    case TAB_CUSTOMER:
        DrawCustomerSection(content, customers, users);
        break;
    case TAB_ORDER:
        DrawOrderSection(content, orders, products, customers);
        break;
    case TAB_ACCOUNT:
        DrawAccountSection(content, users, session);
        break;
    }
}

// =============================================================================
// Staff Dashboard - Browse Products & Place Order
// =============================================================================
// Merges the console's "Browse Products" + "Place Order" into one screen:
// pick a customer, filter/browse the product list, click a row then
// "Order This" to get a small quantity prompt. Reuses the exact same
// stock-validation + placeOrder/reduceStock/saveProducts sequence as the
// Owner dashboard's Order section.

static void DrawStaffBrowseOrderSection(Rectangle area, ProductList &products,
                                        CategoryList &categories,
                                        OrderList &orders, CustomerList &customers)
{
    enum
    {
        FIELD_SEARCH,
        FIELD_CUSTOMER
    };
    static int activeField = -1;

    static char searchBuf[64] = "";
    static char customerIdBuf[16] = "";
    static int selectedIndex = -1;
    static int scrollIndex = 0;
    static string statusMsg;
    static QuantityPromptState qp;
    static int sortPriceActive = 0;   // 0: Default, 1: Low to High, 2: High to Low
    static int selectedCatFilter = 0; // 0: All, or category ID

    static bool customerDropdownEditMode = false;
    static int customerDropdownActive = -1;

    g_staffOrderModalOpen = qp.open;

    if (customerDropdownEditMode || qp.open)
        GuiLock();

    float x = area.x, y = area.y, w = area.width;

    GuiLabel({x, y, w, 24}, "Browse Products & Place Order");
    y += 30;

    // Customer picker - who this order is for.
    GuiLabel({x, y, 110, 18}, "Order for Customer ID:");
    float custTextBoxX = x + 160, custTextBoxW = 80;
    if (GuiTextBox({custTextBoxX, y, custTextBoxW, 26}, customerIdBuf, sizeof(customerIdBuf), activeField == FIELD_CUSTOMER))
        activeField = (activeField == FIELD_CUSTOMER) ? -1 : FIELD_CUSTOMER;
    Rectangle customerDropdownRect = {custTextBoxX + custTextBoxW + 8, y, 220, 26};
    y += 34;

    // Search box - filters the product list by name. Compared
    // case-insensitively (both sides lowercased) so typing "m" matches
    // "Mouse" - a plain find() was case-sensitive and missed it.
    GuiLabel({x, y, 55, 26}, "Search:");
    if (GuiTextBox({x + 60, y, 220, 26}, searchBuf, sizeof(searchBuf), activeField == FIELD_SEARCH))
        activeField = (activeField == FIELD_SEARCH) ? -1 : FIELD_SEARCH;

    // Sort by price control
    GuiLabel({x + 300, y, 80, 26}, "Sort Price:");
    GuiToggleGroup({x + 385, y, 75, 26}, "Default;Low-High;High-Low", &sortPriceActive);
    y += 34;

    // Category filter row with "All" and each category
    vector<Category> allCats = categories.getAllCategories();
    GuiLabel({x, y, 65, 26}, "Category:");

    float catBtnX = x + 70;
    bool allActive = (selectedCatFilter == 0);
    if (GuiToggle({catBtnX, y, 50, 26}, "All", &allActive))
    {
        selectedCatFilter = 0;
    }
    catBtnX += 56;

    for (size_t ci = 0; ci < allCats.size(); ci++)
    {
        float catW = (float)MeasureText(allCats[ci].name.c_str(), 10) + 24.0f;
        if (catW < 60.0f)
            catW = 60.0f;

        if (catBtnX + catW > x + w - 10)
        {
            y += 30;
            catBtnX = x + 70;
        }

        bool isCatActive = (selectedCatFilter == allCats[ci].id);
        if (GuiToggle({catBtnX, y, catW, 26}, allCats[ci].name.c_str(), &isCatActive))
        {
            selectedCatFilter = isCatActive ? allCats[ci].id : 0;
        }
        catBtnX += catW + 6;
    }
    y += 34;

    vector<Product> allItems = products.getAllProducts();
    vector<Product> items;
    string filter(searchBuf);

    string filterLower = filter;
    transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
              [](unsigned char c)
              { return tolower(c); });
    for (size_t i = 0; i < allItems.size(); i++)
    {
        if (selectedCatFilter != 0 && allItems[i].categoryId != selectedCatFilter)
            continue;

        if (filterLower.empty())
        {
            items.push_back(allItems[i]);
        }
        else
        {
            string nameLower = allItems[i].name;
            transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                      [](unsigned char c)
                      { return tolower(c); });
            if (nameLower.find(filterLower) != string::npos)
                items.push_back(allItems[i]);
        }
    }

    if (sortPriceActive == 1) // Low to High
    {
        stable_sort(items.begin(), items.end(), [](const Product &a, const Product &b) {
            return a.price < b.price;
        });
    }
    else if (sortPriceActive == 2) // High to Low
    {
        stable_sort(items.begin(), items.end(), [](const Product &a, const Product &b) {
            return a.price > b.price;
        });
    }

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";

        string catName;
        for (size_t ci = 0; ci < allCats.size(); ci++)
        {
            if (allCats[ci].id == items[i].categoryId)
            {
                catName = allCats[ci].name;
                break;
            }
        }
        string catLabel = catName.empty() ? (" [Cat #" + to_string(items[i].categoryId) + "]")
                                          : (" [" + catName + "]");

        listStr += "#" + to_string(items[i].id) + " " + items[i].name + catLabel +
                   " ($" + FormatMoney(items[i].price) + ", stock " + to_string(items[i].stock) + ")";
    }
    if (listStr.empty())
        listStr = "No matching products";

    if (selectedIndex >= (int)items.size())
        selectedIndex = -1;

    Rectangle listRect = {x, y, w, area.height - (y - area.y) - 90};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &selectedIndex);
    float belowListY = listRect.y + listRect.height + 10;

    if (selectedIndex >= 0 && selectedIndex < (int)items.size())
    {
        if (GuiButton({x, belowListY, 200, 32}, "Order This"))
        {
            qp.open = true;
            g_staffOrderModalOpen = true;
            customerDropdownEditMode = false;
            qp.productId = items[selectedIndex].id;
            qp.productName = items[selectedIndex].name;
            qp.availableStock = items[selectedIndex].stock;
            qp.quantityBuf[0] = '\0';
        }
    }
    belowListY += 40;

    if (!statusMsg.empty())
        GuiLabel({x, belowListY, w, 34}, statusMsg.c_str());

    if (qp.open)
        GuiUnlock();

    int qpResult = DrawQuantityPromptModal(qp);
    if (qpResult == 1)
    {
        try
        {
            int customerId = stoi(string(customerIdBuf));
            int quantity = stoi(string(qp.quantityBuf));
            Product *prod = products.findProduct(qp.productId);

            vector<Customer> allCustomersCheck = customers.getAllCustomers();
            bool customerExists = false;
            for (size_t ci = 0; ci < allCustomersCheck.size(); ci++)
            {
                if (allCustomersCheck[ci].id == customerId)
                {
                    customerExists = true;
                    break;
                }
            }

            if (!customerExists)
            {
                statusMsg = "Customer ID " + to_string(customerId) + " not found.";
            }
            else if (prod == nullptr)
            {
                statusMsg = "That product is no longer available.";
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
                int orderId = orders.placeOrder(customerId, qp.productId, quantity);
                products.reduceStock(qp.productId, quantity);
                products.saveProducts();
                statusMsg = "Order #" + to_string(orderId) + " placed for " +
                            to_string(quantity) + " x \"" + prod->name + "\".";
            }
        }
        catch (...)
        {
            statusMsg = "Enter a valid Customer ID and quantity.";
        }
    }
    else if (qpResult == 2)
    {
        statusMsg = "Order cancelled.";
    }
    g_staffOrderModalOpen = qp.open;

    // Customer dropdown, drawn last so its expanded list isn't covered by
    // the controls above it - BUT skip it entirely while the quantity
    // modal is open. It's drawn after DrawQuantityPromptModal() for that
    // reason, which also means it would otherwise render (and stay
    // clickable) on top of the modal's dim overlay - the stray
    // extra-dropdown-looking control some testers noticed. It reappears
    // normally the instant the modal closes.
    if (!qp.open)
    {
        if (customerDropdownEditMode)
            GuiUnlock();

        vector<Customer> allCustomers = customers.getAllCustomers();
        string customerItems;
        for (size_t i = 0; i < allCustomers.size(); i++)
        {
            if (i > 0)
                customerItems += ";";
            customerItems += to_string(allCustomers[i].id) + " - " + allCustomers[i].name;
        }
        if (customerItems.empty())
            customerItems = "No customers yet";

        int prevCustomerActive = customerDropdownActive;
        if (GuiDropdownBox(customerDropdownRect, customerItems.c_str(), &customerDropdownActive, customerDropdownEditMode))
            customerDropdownEditMode = !customerDropdownEditMode;
        if (customerDropdownActive != prevCustomerActive &&
            customerDropdownActive >= 0 && customerDropdownActive < (int)allCustomers.size())
        {
            snprintf(customerIdBuf, sizeof(customerIdBuf), "%d", allCustomers[customerDropdownActive].id);
        }
    }
}

// =============================================================================
// Staff Dashboard - View Categories (read-only)
// =============================================================================

static void DrawStaffCategorySection(Rectangle area, CategoryList &categories)
{
    static int scrollIndex = 0;
    int dummyActive = -1; // read-only for staff - no edit/delete form

    vector<Category> items = categories.getAllCategories();

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";
        listStr += "#" + to_string(items[i].id) + " " + items[i].name +
                   " - " + items[i].description;
    }
    if (listStr.empty())
        listStr = "No categories yet";

    float x = area.x, y = area.y, w = area.width;
    GuiLabel({x, y, w, 24}, "Categories (view only)");
    y += 30;

    Rectangle listRect = {x, y, w, area.height - 40};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &dummyActive);
}

// =============================================================================
// Staff Dashboard - Customers (view, add, edit - no delete)
// =============================================================================

static void DrawStaffCustomerSection(Rectangle area, CustomerList &customers)
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
    static int editingId = -1; // -1 = "Add" mode
    static string statusMsg;

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

    // Staff can load a customer for editing, but there is no Delete button
    // here - deleting customers stays an Owner-only action.
    if (selectedIndex >= 0 && selectedIndex < (int)items.size())
    {
        if (GuiButton({formX, fy, formW, 32}, "Load for Edit"))
        {
            const Customer &c = items[selectedIndex];
            editingId = c.id;
            CopyIntoBuffer(nameBuf, sizeof(nameBuf), c.name);
            CopyIntoBuffer(phoneBuf, sizeof(phoneBuf), c.phone);
        }
        fy += 40;
    }

    if (!statusMsg.empty())
        GuiLabel({formX, fy, formW, 40}, statusMsg.c_str());
}

// =============================================================================
// Staff Dashboard - View Orders (search by customer)
// =============================================================================

static void DrawStaffOrderSection(Rectangle area, OrderList &orders, CustomerList &customers)
{
    enum
    {
        FIELD_CUSTOMER_SEARCH
    };
    static int activeField = -1;

    static char customerSearchBuf[16] = "";
    static int scrollIndex = 0;
    int dummyActive = -1; // view only - staff doesn't cancel orders here

    float x = area.x, y = area.y, w = area.width;
    GuiLabel({x, y, w, 24}, "Orders (view only)");
    y += 30;

    GuiLabel({x, y, 160, 18}, "Search by Customer ID:");
    if (GuiTextBox({x + 170, y, 100, 26}, customerSearchBuf, sizeof(customerSearchBuf), activeField == FIELD_CUSTOMER_SEARCH))
        activeField = (activeField == FIELD_CUSTOMER_SEARCH) ? -1 : FIELD_CUSTOMER_SEARCH;
    if (GuiButton({x + 280, y, 90, 26}, "Clear"))
        customerSearchBuf[0] = '\0';
    y += 36;

    // Build a customerId -> name lookup once per frame so each row can show
    // a name instead of a bare id - useful since staff won't usually have
    // customer ids memorized.
    vector<Customer> allCustomers = customers.getAllCustomers();

    vector<Order> allItems = orders.getAllOrders();
    vector<Order> items;
    string filter(customerSearchBuf);
    for (size_t i = 0; i < allItems.size(); i++)
    {
        if (filter.empty() || to_string(allItems[i].customerId) == filter)
            items.push_back(allItems[i]);
    }

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";

        string customerLabel = "Cust " + to_string(items[i].customerId);
        for (size_t ci = 0; ci < allCustomers.size(); ci++)
        {
            if (allCustomers[ci].id == items[i].customerId)
            {
                customerLabel = allCustomers[ci].name + " (#" + to_string(items[i].customerId) + ")";
                break;
            }
        }

        listStr += "#" + to_string(items[i].id) + " " + customerLabel +
                   " - Prod " + to_string(items[i].productId) + " x" + to_string(items[i].quantity) +
                   " (" + items[i].date + ")";
    }
    if (listStr.empty())
        listStr = filter.empty() ? "No orders yet" : "No orders for that customer";

    Rectangle listRect = {x, y, w, area.height - (y - area.y) - 10};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &dummyActive);
}

// =============================================================================
// Staff Dashboard - Account (Change Password only)
// =============================================================================

static void DrawStaffAccountSection(Rectangle area, LoginList &users, Session &session)
{
    enum
    {
        FIELD_CURRENT,
        FIELD_NEWPASS,
        FIELD_CONFIRM
    };
    static int activeField = -1;

    static char currentPassBuf[64] = "";
    static char newPassBuf[64] = "";
    static char confirmPassBuf[64] = "";
    static string statusMsg;

    float x = area.x, y = area.y;
    float boxW = 360;

    GuiGroupBox({x, y, boxW, 220}, "Change Password");
    float cpx = x + 15, cpy = y + 20, cpw = boxW - 30;

    GuiLabel({cpx, cpy, cpw, 18}, "Current Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, currentPassBuf, sizeof(currentPassBuf), activeField == FIELD_CURRENT))
        activeField = (activeField == FIELD_CURRENT) ? -1 : FIELD_CURRENT;
    cpy += 32;

    GuiLabel({cpx, cpy, cpw, 18}, "New Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, newPassBuf, sizeof(newPassBuf), activeField == FIELD_NEWPASS))
        activeField = (activeField == FIELD_NEWPASS) ? -1 : FIELD_NEWPASS;
    cpy += 32;

    GuiLabel({cpx, cpy, cpw, 18}, "Confirm New Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, confirmPassBuf, sizeof(confirmPassBuf), activeField == FIELD_CONFIRM))
        activeField = (activeField == FIELD_CONFIRM) ? -1 : FIELD_CONFIRM;
    cpy += 34;

    if (GuiButton({cpx, cpy, cpw, 28}, "Update Password"))
    {
        User *u = users.findUser(session.username);
        string currentPass(currentPassBuf), newPass(newPassBuf), confirmPass(confirmPassBuf);

        if (u == nullptr || u->password != currentPass)
        {
            statusMsg = "Current password is incorrect.";
        }
        else if (newPass.empty())
        {
            statusMsg = "New password cannot be empty.";
        }
        else if (newPass != confirmPass)
        {
            statusMsg = "New passwords do not match.";
        }
        else
        {
            users.editUser(session.username, newPass, u->tag);
            statusMsg = "Password changed successfully.";
            currentPassBuf[0] = '\0';
            newPassBuf[0] = '\0';
            confirmPassBuf[0] = '\0';
        }
    }
    cpy += 32;
    if (!statusMsg.empty())
        GuiLabel({cpx, cpy, cpw, 34}, statusMsg.c_str());
}

// =============================================================================
// Staff Dashboard - sidebar / tab routing
// =============================================================================

static void DrawStaffDashboard(AppScreen &screen, StaffTab &tab,
                               ProductList &products, CategoryList &categories,
                               CustomerList &customers, OrderList &orders,
                               LoginList &users, Session &session)
{
    // Same reasoning as DrawOwnerDashboard: always start unlocked so a
    // dropdown left open on a previous tab can't freeze the sidebar.
    GuiUnlock();

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float sidebarW = 190;

    // Only the Browse & Order tab has a modal (the quantity prompt) -
    // Categories/Orders are read-only and Customers/Account have no
    // delete/confirm step. Same lock-before-sidebar reasoning as
    // DrawOwnerDashboard: without this, "Order This" -> quantity prompt
    // open -> the sidebar (drawn below) would still be clickable through
    // the dimmed overlay.
    bool modalOpenForActiveTab = (tab == STAFF_TAB_BROWSE_ORDER) && g_staffOrderModalOpen;
    if (modalOpenForActiveTab)
        GuiLock();

    GuiPanel({0, 0, sidebarW, (float)screenH}, "Staff Menu");

    float by = 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Browse & Order"))
        tab = STAFF_TAB_BROWSE_ORDER;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Categories"))
        tab = STAFF_TAB_CATEGORY;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Customers"))
        tab = STAFF_TAB_CUSTOMER;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Orders"))
        tab = STAFF_TAB_ORDER;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Account"))
        tab = STAFF_TAB_ACCOUNT;

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
    case STAFF_TAB_BROWSE_ORDER:
        DrawStaffBrowseOrderSection(content, products, categories, orders, customers);
        break;
    case STAFF_TAB_CATEGORY:
        DrawStaffCategorySection(content, categories);
        break;
    case STAFF_TAB_CUSTOMER:
        DrawStaffCustomerSection(content, customers);
        break;
    case STAFF_TAB_ORDER:
        DrawStaffOrderSection(content, orders, customers);
        break;
    case STAFF_TAB_ACCOUNT:
        DrawStaffAccountSection(content, users, session);
        break;
    }
}

// =============================================================================
// Customer Dashboard - Browse Products & Place Order
// =============================================================================

static void DrawCustomerBrowseOrderSection(Rectangle area, ProductList &products,
                                           CategoryList &categories,
                                           OrderList &orders, Session &session)
{
    enum
    {
        FIELD_SEARCH
    };
    static int activeField = -1;

    static char searchBuf[64] = "";
    static int selectedIndex = -1;
    static int scrollIndex = 0;
    static string statusMsg;
    static QuantityPromptState qp;
    static int sortPriceActive = 0;   // 0: Default, 1: Low to High, 2: High to Low
    static int selectedCatFilter = 0; // 0: All, or category ID

    g_customerOrderModalOpen = qp.open;

    if (qp.open)
        GuiLock();

    float x = area.x, y = area.y, w = area.width;

    GuiLabel({x, y, w, 24}, "Browse Products & Place Order");
    y += 30;

    // Search box - filters the product list by name. Compared
    // case-insensitively (both sides lowercased) so typing "m" matches
    // "Mouse" - a plain find() was case-sensitive and missed it.
    GuiLabel({x, y, 55, 26}, "Search:");
    if (GuiTextBox({x + 60, y, 220, 26}, searchBuf, sizeof(searchBuf), activeField == FIELD_SEARCH))
        activeField = (activeField == FIELD_SEARCH) ? -1 : FIELD_SEARCH;

    // Sort by price control
    GuiLabel({x + 300, y, 80, 26}, "Sort Price:");
    GuiToggleGroup({x + 385, y, 75, 26}, "Default;Low-High;High-Low", &sortPriceActive);
    y += 34;

    // Category filter row with "All" and each category
    vector<Category> allCats = categories.getAllCategories();
    GuiLabel({x, y, 65, 26}, "Category:");

    float catBtnX = x + 70;
    bool allActive = (selectedCatFilter == 0);
    if (GuiToggle({catBtnX, y, 50, 26}, "All", &allActive))
    {
        selectedCatFilter = 0;
    }
    catBtnX += 56;

    for (size_t ci = 0; ci < allCats.size(); ci++)
    {
        float catW = (float)MeasureText(allCats[ci].name.c_str(), 10) + 24.0f;
        if (catW < 60.0f)
            catW = 60.0f;

        if (catBtnX + catW > x + w - 10)
        {
            y += 30;
            catBtnX = x + 70;
        }

        bool isCatActive = (selectedCatFilter == allCats[ci].id);
        if (GuiToggle({catBtnX, y, catW, 26}, allCats[ci].name.c_str(), &isCatActive))
        {
            selectedCatFilter = isCatActive ? allCats[ci].id : 0;
        }
        catBtnX += catW + 6;
    }
    y += 34;

    vector<Product> allItems = products.getAllProducts();
    vector<Product> items;
    string filter(searchBuf);

    string filterLower = filter;
    transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
              [](unsigned char c)
              { return tolower(c); });
    for (size_t i = 0; i < allItems.size(); i++)
    {
        if (selectedCatFilter != 0 && allItems[i].categoryId != selectedCatFilter)
            continue;

        if (filterLower.empty())
        {
            items.push_back(allItems[i]);
        }
        else
        {
            string nameLower = allItems[i].name;
            transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                      [](unsigned char c)
                      { return tolower(c); });
            if (nameLower.find(filterLower) != string::npos)
                items.push_back(allItems[i]);
        }
    }

    if (sortPriceActive == 1) // Low to High
    {
        stable_sort(items.begin(), items.end(), [](const Product &a, const Product &b) {
            return a.price < b.price;
        });
    }
    else if (sortPriceActive == 2) // High to Low
    {
        stable_sort(items.begin(), items.end(), [](const Product &a, const Product &b) {
            return a.price > b.price;
        });
    }

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";

        string catName;
        for (size_t ci = 0; ci < allCats.size(); ci++)
        {
            if (allCats[ci].id == items[i].categoryId)
            {
                catName = allCats[ci].name;
                break;
            }
        }
        string catLabel = catName.empty() ? (" [Cat #" + to_string(items[i].categoryId) + "]")
                                          : (" [" + catName + "]");

        listStr += "#" + to_string(items[i].id) + " " + items[i].name + catLabel +
                   " ($" + FormatMoney(items[i].price) + ", stock " + to_string(items[i].stock) + ")";
    }
    if (listStr.empty())
        listStr = "No matching products";

    if (selectedIndex >= (int)items.size())
        selectedIndex = -1;

    Rectangle listRect = {x, y, w, area.height - (y - area.y) - 90};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &selectedIndex);
    float belowListY = listRect.y + listRect.height + 10;

    if (selectedIndex >= 0 && selectedIndex < (int)items.size())
    {
        if (GuiButton({x, belowListY, 200, 32}, "Order This"))
        {
            qp.open = true;
            g_customerOrderModalOpen = true;
            qp.productId = items[selectedIndex].id;
            qp.productName = items[selectedIndex].name;
            qp.availableStock = items[selectedIndex].stock;
            qp.quantityBuf[0] = '\0';
        }
    }
    belowListY += 40;

    if (!statusMsg.empty())
        GuiLabel({x, belowListY, w, 34}, statusMsg.c_str());

    if (qp.open)
        GuiUnlock();

    int qpResult = DrawQuantityPromptModal(qp);
    if (qpResult == 1)
    {
        try
        {
            int quantity = stoi(string(qp.quantityBuf));
            Product *prod = products.findProduct(qp.productId);

            if (prod == nullptr)
            {
                statusMsg = "That product is no longer available.";
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
                int orderId = orders.placeOrder(session.customerId, qp.productId, quantity);
                products.reduceStock(qp.productId, quantity);
                products.saveProducts();
                statusMsg = "Order #" + to_string(orderId) + " placed for " +
                            to_string(quantity) + " x \"" + prod->name + "\".";
            }
        }
        catch (...)
        {
            statusMsg = "Quantity must be a valid number.";
        }
    }
    else if (qpResult == 2)
    {
        statusMsg = "Order cancelled.";
    }
    g_customerOrderModalOpen = qp.open;
}

// =============================================================================
// Customer Dashboard - My Orders (read-only, filtered to current customer)
// =============================================================================

static void DrawCustomerOrdersSection(Rectangle area, OrderList &orders, Session &session)
{
    static int scrollIndex = 0;
    int dummyActive = -1; // view only - customer does not cancel orders here

    float x = area.x, y = area.y, w = area.width;
    GuiLabel({x, y, w, 24}, "My Orders");
    y += 30;

    vector<Order> allItems = orders.getAllOrders();
    vector<Order> items;
    for (size_t i = 0; i < allItems.size(); i++)
    {
        if (allItems[i].customerId == session.customerId)
            items.push_back(allItems[i]);
    }

    string listStr;
    for (size_t i = 0; i < items.size(); i++)
    {
        if (i > 0)
            listStr += ";";

        listStr += "#" + to_string(items[i].id) +
                   " - Prod " + to_string(items[i].productId) + " x" + to_string(items[i].quantity) +
                   " (" + items[i].date + ")";
    }
    if (listStr.empty())
        listStr = "No orders yet";

    Rectangle listRect = {x, y, w, area.height - (y - area.y) - 10};
    GuiListView(listRect, listStr.c_str(), &scrollIndex, &dummyActive);
}

// =============================================================================
// Customer Dashboard - Account (Change Password only)
// =============================================================================

static void DrawCustomerAccountSection(Rectangle area, LoginList &users, Session &session)
{
    enum
    {
        FIELD_CURRENT,
        FIELD_NEWPASS,
        FIELD_CONFIRM
    };
    static int activeField = -1;

    static char currentPassBuf[64] = "";
    static char newPassBuf[64] = "";
    static char confirmPassBuf[64] = "";
    static string statusMsg;

    float x = area.x, y = area.y;
    float boxW = 360;

    GuiGroupBox({x, y, boxW, 220}, "Change Password");
    float cpx = x + 15, cpy = y + 20, cpw = boxW - 30;

    GuiLabel({cpx, cpy, cpw, 18}, "Current Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, currentPassBuf, sizeof(currentPassBuf), activeField == FIELD_CURRENT))
        activeField = (activeField == FIELD_CURRENT) ? -1 : FIELD_CURRENT;
    cpy += 32;

    GuiLabel({cpx, cpy, cpw, 18}, "New Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, newPassBuf, sizeof(newPassBuf), activeField == FIELD_NEWPASS))
        activeField = (activeField == FIELD_NEWPASS) ? -1 : FIELD_NEWPASS;
    cpy += 32;

    GuiLabel({cpx, cpy, cpw, 18}, "Confirm New Password:");
    cpy += 20;
    if (GuiTextBox({cpx, cpy, cpw, 26}, confirmPassBuf, sizeof(confirmPassBuf), activeField == FIELD_CONFIRM))
        activeField = (activeField == FIELD_CONFIRM) ? -1 : FIELD_CONFIRM;
    cpy += 34;

    if (GuiButton({cpx, cpy, cpw, 28}, "Update Password"))
    {
        User *u = users.findUser(session.username);
        string currentPass(currentPassBuf), newPass(newPassBuf), confirmPass(confirmPassBuf);

        if (u == nullptr || u->password != currentPass)
        {
            statusMsg = "Current password is incorrect.";
        }
        else if (newPass.empty())
        {
            statusMsg = "New password cannot be empty.";
        }
        else if (newPass != confirmPass)
        {
            statusMsg = "New passwords do not match.";
        }
        else
        {
            users.editUser(session.username, newPass, u->tag);
            statusMsg = "Password changed successfully.";
            currentPassBuf[0] = '\0';
            newPassBuf[0] = '\0';
            confirmPassBuf[0] = '\0';
        }
    }
    cpy += 32;
    if (!statusMsg.empty())
        GuiLabel({cpx, cpy, cpw, 34}, statusMsg.c_str());
}

// =============================================================================
// Customer Dashboard - sidebar / tab routing
// =============================================================================

static void DrawCustomerDashboard(AppScreen &screen, CustomerTab &tab,
                                  ProductList &products, CategoryList &categories,
                                  CustomerList &customers, OrderList &orders,
                                  LoginList &users, Session &session)
{
    (void)customers;
    // Same reasoning as DrawOwnerDashboard: always start unlocked so a
    // dropdown left open on a previous tab can't freeze the sidebar.
    GuiUnlock();

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float sidebarW = 190;

    // Only the Browse & Order tab has a modal (the quantity prompt).
    // Lock the sidebar before drawing it if a modal is open.
    bool modalOpenForActiveTab = (tab == CUSTOMER_TAB_BROWSE_ORDER) && g_customerOrderModalOpen;
    if (modalOpenForActiveTab)
        GuiLock();

    GuiPanel({0, 0, sidebarW, (float)screenH}, "Customer Menu");

    float by = 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Browse & Order"))
        tab = CUSTOMER_TAB_BROWSE_ORDER;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "My Orders"))
        tab = CUSTOMER_TAB_ORDERS;
    by += 40;
    if (GuiButton({10, by, sidebarW - 20, 32}, "Account"))
        tab = CUSTOMER_TAB_ACCOUNT;

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
    case CUSTOMER_TAB_BROWSE_ORDER:
        DrawCustomerBrowseOrderSection(content, products, categories, orders, session);
        break;
    case CUSTOMER_TAB_ORDERS:
        DrawCustomerOrdersSection(content, orders, session);
        break;
    case CUSTOMER_TAB_ACCOUNT:
        DrawCustomerAccountSection(content, users, session);
        break;
    }
}

static void DrawCustomerDashboard(AppScreen &screen, CustomerTab &tab,
                                  ProductList &products, OrderList &orders,
                                  LoginList &users, Session &session)
{
    CategoryList categories;
    CustomerList customers;
    DrawCustomerDashboard(screen, tab, products, categories, customers, orders, users, session);
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
            else if (u->tag == "staff")
            {
                statusMsg = "";
                screen = SCREEN_STAFF;
            }
            else if (u->tag == "customer")
            {
                statusMsg = "";
                screen = SCREEN_CUSTOMER;
            }
            else
            {
                statusMsg = "Logged in as \"" + u->tag + "\" - no dashboard is built for that role yet.";
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
    StaffTab staffTab = STAFF_TAB_BROWSE_ORDER;
    CustomerTab customerTab = CUSTOMER_TAB_BROWSE_ORDER;
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
        case SCREEN_STAFF:
            DrawStaffDashboard(screen, staffTab, products, categories, customers, orders, users, session);
            break;
        case SCREEN_CUSTOMER:
            DrawCustomerDashboard(screen, customerTab, products, categories, customers, orders, users, session);
            break;
        }

        EndDrawing();
    }

    products.saveProducts();
    CloseWindow();
    return 0;
}