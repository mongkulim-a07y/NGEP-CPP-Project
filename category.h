#ifndef category_h
#define category_h
#include<iostream>
#include<vector>
#include<string>
#include<fstream>
using namespace std;
#include "type.h"


class CategoryMananger {
    private:
    vector<string> categories;

    public:
    CategoryMananger() {
        loadCategories();
    }

    //Save categories to file
    void saveCategories(){
        ofstream file("CsvFile/categories.txt");
        for(string name: categories){
            file << name << endl;
        }
        file.close();
    }
    // Add a category
    void addCategory(){
        int n;
        cout << "How many categories fo you want to add?";
        cin >> n;

        cin.ignore();
        for(int i=0; i<n; i++){
            string name;
            cout << "Enter category " << i+1 << ":";
            getline(cin, name);
            categories.push_back(name);
        }
        
        saveCategories();

        cout << "Category added successfully.";
    }
    // View all Categories
    void viewCategories(){
       
        
        cout << "\n===== Categories =====\n";

        if (categories.empty()) {
            cout << "No categories found.\n";
            return;
        }

        for (int i = 0; i < categories.size(); i++) {
            cout << i + 1 << ". " << categories[i] << endl;
        }
    }

    // Edit a category
    void editCategory(){
        viewCategories();
        if(categories.empty()){
            return;
        }
        int index;

        cout << "Enter category number to edit: " ;
        cin >> index;

        if(index < 1 || index > categories.size()){
            cout << "Invaid category number." << endl;
            return;
        }

        cin.ignore();
        
        string newName;
        
        cout << "Enter new category name:";
        getline(cin, newName);
        
        categories[index-1]=newName;
        saveCategories();

        cout << "Category updated successfully!" << endl;

    }
    // Delete a category
    void deleteCategory(){
        viewCategories();
        if(categories.empty()){
            return;
        }

        int index;
        
        cout << "Enter category number to delete: " ;
        cin >> index;
        
        if(index <1 || index > categories.size()){
            cout << "Invalid category number." << endl;
            return;
        }

        categories.erase(categories.begin()+index-1);

        saveCategories();

        cout << "Category deleted successfully!" << endl;
    }
    // Load categories from file
    void loadCategories(){
        ifstream file("CsvFile/categories.txt");
        string name;
        
        if (!file.is_open()){
            cout << "error";
            return;
        }
        while(getline(file, name)){
            categories.push_back(name);
        }

        file.close();
    }
};


#endif 