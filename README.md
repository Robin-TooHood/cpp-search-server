# cpp-search-server

Final student project: search server

# About

This is a search server that, for a given query, calculates the most relevant results using TF-IDF.
In this project, I worked with various algorithms and main C++ libraries.
When creating a search server, you can set stop words that will not be taken into account in calculations. The project also has the functionality of deduplicating identical documents and processing incorrect requests or adding incorrect documents.

# Deployment Instructions and System Requirements

The project is written in VS Code and uses C++17, so to run it you need:
1. IDE
2. Compiler

Deployment example:
* Download and install VS Code [code.visualstudio.com](https://code.visualstudio.com/)
* Install the C++ extension in VS Code
* Install the latest MinGW release [winlibs.com](https://winlibs.com/#download-release) and add it to PATH
* Clone the repository
* Run main.cpp file with g++ build (you can add the following code in main function to test deduplicating for example):

    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});

    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;

    RemoveDuplicates(search_server);

    cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;

* At first run, a ".vscode" folder with "tasks.json" file will be automatically created and you need to change the line in the "args" section from ***"${file}",*** to ***"${fileDirname}/*.cpp",** (so VS Code merge all files in a folder into one program when compiled)
* Run it again
* If you used example code above output should look like this:

    Before duplicates removed: 9

    Found duplicate document id 3

    Found duplicate document id 4

    Found duplicate document id 5

    Found duplicate document id 7
    
    After duplicates removed: 5
