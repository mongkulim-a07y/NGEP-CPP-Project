#ifndef loginlist_hpp
#define loginlist_hpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <ctime>
#include <vector>
using namespace std;

// If you already have a User struct in type.h, remove this block
// and #include "type.h" instead, like categoryt.hpp does for Category.
struct User
{
    string username;
    string password; // NOTE: stored in plain text here, same as Category's CSV style.
                     // For anything beyond a class project, hash this before saving.
    string tag;      // "admin", "staff", or "customer"
    int id;          // only meaningful when tag == "customer" (0 otherwise)
};

struct LoginHistoryEntry
{
    string username;
    string tag;
    string datetime;
};

class LoginList
{
private:
    struct UserNode
    {
        User data;
        UserNode *next;

        UserNode(User value) : data(value), next(nullptr) {}
    };

    UserNode *head;
    UserNode *tail;

    static constexpr const char *HISTORY_CSV_PATH = "CsvFile/loginHistory.csv";

public:
    LoginList() : head(nullptr), tail(nullptr)
    {
        loadUsers();
    }
    ~LoginList()
    {
        freeUsers();
    }

    // Prevent accidental copies (raw pointers would double-free otherwise)
    LoginList(const LoginList &) = delete;
    LoginList &operator=(const LoginList &) = delete;

    void freeUsers()
    {
        UserNode *current = head;
        while (current != nullptr)
        {
            UserNode *temp = current;
            current = current->next;
            delete temp;
        }
        head = nullptr;
        tail = nullptr;
    }

    // 1. LOAD: Read all users from CSV into the linked list
    void loadUsers()
    {
        ifstream file;
        file.open("CsvFile/users.csv");
        if (!file.is_open())
            return;

        string line;
        while (getline(file, line))
        {
            if (line.empty())
                continue;

            stringstream ss(line);
            string username, password, tag, idStr;

            if (getline(ss, username, ',') &&
                getline(ss, password, ',') &&
                getline(ss, tag, ',') &&
                getline(ss, idStr))
            {
                if (username == "Username" || username == "username")
                    continue; // Skip CSV header

                try
                {
                    User u;
                    u.username = username;
                    u.password = password;
                    u.tag = tag;
                    u.id = idStr.empty() ? 0 : stoi(idStr);

                    UserNode *newNode = new UserNode(u);
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
    void saveUsers()
    {
        ofstream file;
        file.open("CsvFile/users.csv");
        if (!file.is_open())
        {
            cout << "Error: Could not open file for writing.\n";
            return;
        }

        file << "Username,Password,Tag,ID\n";

        UserNode *temp = head;
        while (temp != nullptr)
        {
            file << temp->data.username << ","
                 << temp->data.password << ","
                 << temp->data.tag << ","
                 << temp->data.id << "\n";
            temp = temp->next;
        }

        file.close();
        cout << "Users saved successfully.\n";
    }

    // 3. ADD: Register a new user (checks for duplicate username first)
    bool addUser(const string &username, const string &password, const string &tag, int id = 0)
    {
        if (findUser(username) != nullptr)
        {
            cout << "Username already exists!\n";
            return false;
        }

        User u{username, password, tag, tag == "customer" ? id : 0};
        UserNode *newNode = new UserNode(u);

        if (head == nullptr)
        {
            head = tail = newNode;
        }
        else
        {
            tail->next = newNode;
            tail = newNode;
        }

        saveUsers(); // persist immediately
        cout << "User registered successfully!\n";
        return true;
    }

    // 4. FIND: Look up a user by username (nullptr if not found)
    User *findUser(const string &username)
    {
        UserNode *temp = head;
        while (temp != nullptr)
        {
            if (temp->data.username == username)
                return &temp->data;
            temp = temp->next;
        }
        return nullptr;
    }

    // Look up the login belonging to a specific customer (tag == "customer"
    // and matching id). Used when deleting a customer, so their login can
    // be removed too instead of being left behind as an orphan.
    User *findUserByCustomerId(int customerId)
    {
        UserNode *temp = head;
        while (temp != nullptr)
        {
            if (temp->data.tag == "customer" && temp->data.id == customerId)
                return &temp->data;
            temp = temp->next;
        }
        return nullptr;
    }

    // 5. LOGIN: Check credentials. Returns pointer to the user on success, nullptr on failure.
    User *login(const string &username, const string &password)
    {
        User *u = findUser(username);
        if (u == nullptr)
        {
            cout << "Login failed: username not found.\n";
            return nullptr;
        }
        if (u->password != password)
        {
            cout << "Login failed: incorrect password.\n";
            return nullptr;
        }
        cout << "Login successful! Welcome, " << u->username << " (" << u->tag << ")\n";
        logLoginHistory(u->username, u->tag);
        return u;
    }

    // Returns the current date and time as "YYYY-MM-DD HH:MM:SS",
    // same format/approach as OrderList::getCurrentDateTime().
    string getCurrentDateTime() const
    {
        time_t now = time(0);
        tm *ltm = localtime(&now);

        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);

        return string(buffer);
    }

    // Appends one row to CsvFile/loginHistory.csv for a successful login.
    // This is an append-only audit log, not an in-memory list like the
    // users themselves: nothing here is ever edited or deleted, so there's
    // no need to load it into memory or track it with a linked list.
    void logLoginHistory(const string &username, const string &tag)
    {
        // Only write the header once, the first time the file is created.
        ifstream check(HISTORY_CSV_PATH);
        bool needsHeader = !check.good();
        check.close();

        ofstream file(HISTORY_CSV_PATH, ios::app);
        if (!file.is_open())
        {
            cout << "[Error] Could not open " << HISTORY_CSV_PATH << " to log login history.\n";
            return;
        }

        if (needsHeader)
            file << "Username,Tag,DateTime\n";

        file << username << "," << tag << "," << getCurrentDateTime() << "\n";
        file.close();
    }

    // Returns every recorded login as data, oldest first. Needed by
    // anything that needs to enumerate the log (e.g. a GUI list) rather
    // than just print it via viewLoginHistory().
    vector<LoginHistoryEntry> getLoginHistory() const
    {
        vector<LoginHistoryEntry> result;
        ifstream file(HISTORY_CSV_PATH);
        if (!file.is_open())
            return result;

        string line;
        while (getline(file, line))
        {
            if (line.empty())
                continue;

            stringstream ss(line);
            string username, tag, datetime;
            if (getline(ss, username, ',') && getline(ss, tag, ',') && getline(ss, datetime))
            {
                if (username == "Username") // skip header row
                    continue;

                LoginHistoryEntry entry;
                entry.username = username;
                entry.tag = tag;
                entry.datetime = datetime;
                result.push_back(entry);
            }
        }
        file.close();
        return result;
    }

    // Displays every recorded login, oldest first. Read-only, no CRUD -
    // matches the "audit log" nature of this data.
    void viewLoginHistory() const
    {
        ifstream file(HISTORY_CSV_PATH);
        if (!file.is_open())
        {
            cout << "\n[Info] No login history found yet.\n";
            return;
        }

        cout << "\n--------------------------------------------------\n";
        cout << setw(30) << "LOGIN HISTORY" << "\n";
        cout << "--------------------------------------------------\n";
        cout << left << setw(20) << "Username"
             << setw(10) << "Tag"
             << setw(20) << "Date & Time" << "\n";
        cout << "--------------------------------------------------\n";

        string line;
        int count = 0;
        while (getline(file, line))
        {
            if (line.empty())
                continue;

            stringstream ss(line);
            string username, tag, datetime;
            if (getline(ss, username, ',') && getline(ss, tag, ',') && getline(ss, datetime))
            {
                if (username == "Username") // skip header row
                    continue;

                cout << left << setw(20) << username
                     << setw(10) << tag
                     << setw(20) << datetime << "\n";
                count++;
            }
        }
        file.close();

        cout << "--------------------------------------------------\n";
        cout << "Total Logins Recorded: " << count << "\n";
        cout << "--------------------------------------------------\n";
    }

    // 6. EDIT: Update password and/or tag for an existing user
    bool editUser(const string &username, const string &newPassword, const string &newTag)
    {
        User *u = findUser(username);
        if (u == nullptr)
        {
            cout << "Username " << username << " not found!\n";
            return false;
        }
        u->password = newPassword;
        u->tag = newTag;
        saveUsers();
        cout << "User " << username << " updated successfully!\n";
        return true;
    }

    // 7. DELETE: Remove a user by username
    bool deleteUser(const string &username)
    {
        if (head == nullptr)
        {
            cout << "User list is empty!\n";
            return false;
        }

        UserNode *temp = head;
        UserNode *prev = nullptr;

        while (temp != nullptr && temp->data.username != username)
        {
            prev = temp;
            temp = temp->next;
        }

        if (temp == nullptr)
        {
            cout << "Username " << username << " not found!\n";
            return false;
        }

        if (temp == head)
        {
            head = head->next;
            if (head == nullptr)
                tail = nullptr;
        }
        else
        {
            prev->next = temp->next;
            if (temp == tail)
                tail = prev;
        }

        delete temp;
        saveUsers();
        cout << "User " << username << " deleted successfully!\n";
        return true;
    }

    // 8. VIEW: Display all users (for admin screens)
    void viewUsers() const
    {
        cout << "\n+----------------------+----------------------+----------+-----+\n";
        cout << "| " << left << setw(20) << "Username"
             << " | " << left << setw(20) << "Password"
             << " | " << left << setw(8) << "Tag"
             << " | " << left << setw(3) << "ID" << " |\n";
        cout << "+----------------------+----------------------+----------+-----+\n";

        if (head == nullptr)
        {
            cout << "| " << left << setw(63) << "No users found." << " |\n";
        }
        else
        {
            UserNode *current = head;
            while (current != nullptr)
            {
                cout << "| " << left << setw(20) << current->data.username
                     << " | " << left << setw(20) << current->data.password
                     << " | " << left << setw(8) << current->data.tag
                     << " | " << left << setw(3) << current->data.id << " |\n";
                current = current->next;
            }
        }
        cout << "+----------------------+----------------------+----------+-----+\n";
    }

    int getUserCount() const
    {
        int count = 0;
        UserNode *current = head;
        while (current != nullptr)
        {
            count++;
            current = current->next;
        }
        return count;
    }
};

#endif