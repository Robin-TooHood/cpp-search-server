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
* Run main.cpp file with g++ build:
* At first run, a ".vscode" folder with "tasks.json" file will be automatically created and you need to change the line in the "args" section from `"${file}",` to `"${fileDirname}/*.cpp",` (so VS Code merge all files in a folder into one program when compiled)
* Run it again
* Output should look like this:

```

ACTUAL by default:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }
{ document_id = 1, relevance = 0.173287, rating = 1 }
{ document_id = 3, relevance = 0.173287, rating = 1 }
BANNED:
Even ids:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }

```